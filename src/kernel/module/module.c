#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/module/module.h>

#include <kernel/fs/vfs.h>
#include <kernel/init/boot_info.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/symbol.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>
#include <kernel/version.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/elf.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/map.h>
#include <sys/status.h>

static module_info_t fakeKernelModuleInfo = {
    .name = "kernel",
    .author = "The PatchworkOS Authors",
    .description = "The PatchworkOS Kernel",
    .version = OS_VERSION,
    .license = "MIT",
    .osVersion = OS_VERSION,
    .deviceTypes = "",
};

static bool module_name_cmp(map_entry_t* entry, const void* key)
{
    module_t* m = CONTAINER_OF(entry, module_t, mapEntry);
    return strcmp(m->info.name, (const char*)key) == 0;
}

static bool module_provider_cmp(map_entry_t* entry, const void* key)
{
    module_t* m = CONTAINER_OF(entry, module_t, providerEntry);
    return m->symbolGroupId == *(const symbol_group_id_t*)key;
}

static bool device_name_cmp(map_entry_t* entry, const void* key)
{
    module_device_t* d = CONTAINER_OF(entry, module_device_t, mapEntry);
    return strcmp(d->name, (const char*)key) == 0;
}

static bool symbol_cache_cmp(map_entry_t* entry, const void* key)
{
    module_cached_symbol_t* s = CONTAINER_OF(entry, module_cached_symbol_t, mapEntry);
    return strcmp(s->name, (const char*)key) == 0;
}

static bool device_cache_cmp(map_entry_t* entry, const void* key)
{
    module_cached_device_t* d = CONTAINER_OF(entry, module_cached_device_t, mapEntry);
    return strcmp(d->type, (const char*)key) == 0;
}

static list_t modulesList = LIST_CREATE(modulesList);

static MAP_CREATE(modulesMap, 64, module_name_cmp);
static MAP_CREATE(providerMap, 64, module_provider_cmp);
static MAP_CREATE(deviceMap, 64, device_name_cmp);
static MAP_CREATE(symbolCache, 1024, symbol_cache_cmp);
static MAP_CREATE(deviceCache, 256, device_cache_cmp);

static bool cacheValid = false;

static mutex_t lock = MUTEX_CREATE(lock);

typedef struct
{
    file_t* dir;
    const char* filename;
    process_t* process;
    module_t* current; ///< The module whose dependencies are currently being loaded.
    list_t dependencies;
} module_load_ctx_t;

static void* module_resolve_symbol_callback(const char* name, void* data);
static status_t module_load_dependency(module_load_ctx_t* ctx, const char* symbolName);

#define MODULE_SYMBOL_ALLOWED(type, binding, name) \
    (((type) == STT_OBJECT || (type) == STT_FUNC) && ((binding) == STB_GLOBAL) && \
        (strncmp(name, MODULE_RESERVED_PREFIX, MODULE_RESERVED_PREFIX_LENGTH) != 0))

static module_t* module_new(module_info_t* info)
{
    module_t* module = malloc(sizeof(module_t) + info->dataSize);
    if (module == NULL)
    {
        return NULL;
    }
    list_entry_init(&module->listEntry);
    map_entry_init(&module->mapEntry);
    map_entry_init(&module->providerEntry);
    list_entry_init(&module->gcEntry);
    list_entry_init(&module->loadEntry);
    module->flags = MODULE_FLAG_NONE;
    module->baseAddr = NULL;
    module->size = 0;
    module->procedure = NULL;
    module->symbolGroupId = symbol_generate_group_id();
    list_init(&module->dependencies);
    list_init(&module->deviceHandlers);
    memcpy_s(&module->info, sizeof(module_info_t) + info->dataSize, info, sizeof(module_info_t) + info->dataSize);

    // Since the info strings are stored as pointers into the info data, we need to adjust them to point into our own
    // copy.
    intptr_t offset = (intptr_t)module->info.data - (intptr_t)info->data;
    module->info.name = (char*)((uintptr_t)module->info.name + offset);
    module->info.author = (char*)((uintptr_t)module->info.author + offset);
    module->info.description = (char*)((uintptr_t)module->info.description + offset);
    module->info.version = (char*)((uintptr_t)module->info.version + offset);
    module->info.license = (char*)((uintptr_t)module->info.license + offset);
    module->info.osVersion = (char*)((uintptr_t)module->info.osVersion + offset);
    module->info.deviceTypes = (char*)((uintptr_t)module->info.deviceTypes + offset);

    list_push_back(&modulesList, &module->listEntry);

    if (map_find(&modulesMap, info->name, hash_string(info->name)) != NULL)
    {
        goto error;
    }

    map_insert(&modulesMap, &module->mapEntry, hash_string(info->name));
    map_insert(&providerMap, &module->providerEntry, hash_uint64(module->symbolGroupId));

    return module;

error:
    list_remove(&module->listEntry);
    free(module);
    return NULL;
}

static void module_free(module_t* module)
{
    LOG_DEBUG("freeing resources for module '%s'\n", module->info.name);

    assert(!(module->flags & MODULE_FLAG_LOADED));

    list_remove(&module->listEntry);
    map_remove(&modulesMap, &module->mapEntry, hash_string(module->info.name));
    map_remove(&providerMap, &module->providerEntry, hash_uint64(module->symbolGroupId));

    symbol_remove_group(module->symbolGroupId);

    if (module->baseAddr != NULL)
    {
        vmm_unmap(NULL, module->baseAddr, module->size);
    }

    free(module);
}

static status_t module_call_load_event(module_t* module)
{
    LOG_DEBUG("calling load event for module '%s'\n", module->info.name);
    module_event_t loadEvent = {
        .type = MODULE_EVENT_LOAD,
    };
    status_t status = module->procedure(&loadEvent);
    if (IS_ERR(status))
    {
        LOG_ERR("call to load event for module '%s' failed\n", module->info.name);
        return status;
    }
    module->flags |= MODULE_FLAG_LOADED;
    return OK;
}

static void module_call_unload_event(module_t* module)
{
    if (module->flags & MODULE_FLAG_LOADED)
    {
        LOG_DEBUG("calling unload event for module '%s'\n", module->info.name);
        module_event_t unloadEvent = {
            .type = MODULE_EVENT_UNLOAD,
        };
        module->procedure(&unloadEvent);
        module->flags &= ~MODULE_FLAG_LOADED;
    }
}

static module_t* module_find_by_name(const char* name)
{
    map_entry_t* entry = map_find(&modulesMap, name, hash_string(name));
    return entry ? CONTAINER_OF(entry, module_t, mapEntry) : NULL;
}

static module_t* module_find_provider(symbol_group_id_t groupId)
{
    map_entry_t* entry = map_find(&providerMap, &groupId, hash_uint64(groupId));
    return entry ? CONTAINER_OF(entry, module_t, providerEntry) : NULL;
}

static module_device_t* module_device_new(const char* type, const char* name)
{
    module_device_t* device = malloc(sizeof(module_device_t));
    if (device == NULL)
    {
        return NULL;
    }
    map_entry_init(&device->mapEntry);
    strncpy_s(device->type, MODULE_MAX_DEVICE_STRING, type, MODULE_MAX_DEVICE_STRING);
    strncpy_s(device->name, MODULE_MAX_DEVICE_STRING, name, MODULE_MAX_DEVICE_STRING);
    list_init(&device->handlers);

    map_insert(&deviceMap, &device->mapEntry, hash_string(name));

    return device;
}

static void module_device_free(module_device_t* device)
{
    assert(list_is_empty(&device->handlers));

    map_remove(&deviceMap, &device->mapEntry, hash_string(device->name));
    free(device);
}

static module_device_t* module_device_get(const char* name)
{
    map_entry_t* entry = map_find(&deviceMap, name, hash_string(name));
    return entry ? CONTAINER_OF(entry, module_device_t, mapEntry) : NULL;
}

static inline status_t module_handler_add(module_device_handler_t** out, module_t* module, module_device_t* device)
{
    module_device_handler_t* handler = malloc(sizeof(module_device_handler_t));
    if (handler == NULL)
    {
        return ERR(MODULE, NOMEM);
    }
    list_entry_init(&handler->deviceEntry);
    list_entry_init(&handler->moduleEntry);
    list_entry_init(&handler->loadEntry);
    handler->module = module;
    handler->device = device;

    module_event_t attachEvent = {
        .type = MODULE_EVENT_DEVICE_ATTACH,
        .deviceAttach.type = device->type,
        .deviceAttach.name = device->name,
    };
    status_t status = module->procedure(&attachEvent);
    if (IS_ERR(status))
    {
        LOG_ERR("call to attach event for module '%s' failed\n", module->info.name);
        free(handler);
        return status;
    }

    list_push_back(&device->handlers, &handler->deviceEntry);
    list_push_back(&module->deviceHandlers, &handler->moduleEntry);

    *out = handler;
    return OK;
}

static inline void module_handler_remove(module_device_handler_t* handler)
{
    module_event_t detachEvent = {
        .type = MODULE_EVENT_DEVICE_DETACH,
        .deviceDetach.type = handler->device->type,
        .deviceDetach.name = handler->device->name,
    };
    handler->module->procedure(&detachEvent);

    list_remove(&handler->deviceEntry);
    list_remove(&handler->moduleEntry);
    free(handler);
}

/**
 * @brief Copy a string up to either a null-terminator or a semicolon into the output buffer.
 */
static inline uint64_t module_string_copy(const char* str, char* out, size_t outSize)
{
    if (outSize == 0)
    {
        return 0;
    }

    size_t len = 0;
    while (str[len] != '\0' && str[len] != ';' && len < outSize - 1)
    {
        len++;
    }
    strncpy_s(out, outSize, str, len);
    return len;
}

static inline status_t module_info_parse(module_info_t** out, const char* moduleInfo)
{
    size_t totalSize = strnlen_s(moduleInfo, MODULE_MAX_INFO);
    if (totalSize < MODULE_MIN_INFO || totalSize >= MODULE_MAX_INFO)
    {
        LOG_ERR("module info string is of invalid size %zu\n", totalSize);
        return ERR(MODULE, INVAL);
    }

    module_info_t* info = malloc(sizeof(module_info_t) + totalSize + 1);
    if (info == NULL)
    {
        return ERR(MODULE, NOMEM);
    }

    size_t offset = 0;
    info->name = &info->data[offset];
    size_t parsed = module_string_copy(&moduleInfo[offset], info->name, MODULE_MAX_NAME);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module name\n");
        goto error;
    }
    info->name[parsed] = '\0';
    offset += parsed + 1;

    info->author = &info->data[offset];
    parsed = module_string_copy(&moduleInfo[offset], info->author, MODULE_MAX_AUTHOR);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module author\n");
        goto error;
    }
    info->author[parsed] = '\0';
    offset += parsed + 1;

    info->description = &info->data[offset];
    parsed = module_string_copy(&moduleInfo[offset], info->description, MODULE_MAX_DESCRIPTION);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module description\n");
        goto error;
    }
    info->description[parsed] = '\0';
    offset += parsed + 1;

    info->version = &info->data[offset];
    parsed = module_string_copy(&moduleInfo[offset], info->version, MODULE_MAX_VERSION);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module version\n");
        goto error;
    }
    info->version[parsed] = '\0';
    offset += parsed + 1;

    info->license = &info->data[offset];
    parsed = module_string_copy(&moduleInfo[offset], info->license, MODULE_MAX_LICENSE);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module license\n");
        goto error;
    }
    info->license[parsed] = '\0';
    offset += parsed + 1;

    info->osVersion = &info->data[offset];
    parsed = module_string_copy(&moduleInfo[offset], info->osVersion, MODULE_MAX_VERSION);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module OS version\n");
        goto error;
    }
    info->osVersion[parsed] = '\0';
    offset += parsed + 1;

    if (strcmp(info->osVersion, OS_VERSION) != 0)
    {
        LOG_ERR("module '%s' requires OS version '%s' but running version is '%s'\n", info->name, info->osVersion,
            OS_VERSION);
        goto error;
    }

    size_t deviceTypesLength = totalSize - offset;
    info->deviceTypes = &info->data[offset];
    strncpy_s(info->deviceTypes, deviceTypesLength + 1, &moduleInfo[offset], deviceTypesLength + 1);

    info->dataSize = totalSize + 1;
    *out = info;
    return OK;

error:
    free(info);
    return ERR(MODULE, INVALELF);
}

typedef struct
{
    Elf64_File elf;
    module_info_t* info;
} module_file_t;

static status_t module_file_read(module_file_t* outFile, const path_t* dirPath, process_t* process,
    const char* filename)
{
    pathname_t pathname;
    status_t status = pathname_init(&pathname, filename);
    if (IS_ERR(status))
    {
        return status;
    }

    file_t* file;
    status = vfs_openat(&file, dirPath, &pathname, process);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(file);

    size_t fileSize;
    vfs_seek(file, 0, SEEK_END, &fileSize);
    vfs_seek(file, 0, SEEK_SET, NULL);

    uint8_t* fileData = malloc(fileSize);
    if (fileData == NULL)
    {
        return ERR(MODULE, NOMEM);
    }

    size_t bytesRead;
    status = vfs_read(file, fileData, fileSize, &bytesRead);
    if (IS_ERR(status))
    {
        free(fileData);
        return status;
    }

    if (bytesRead != fileSize)
    {
        free(fileData);
        return ERR(MODULE, TOCTOU);
    }

    if (elf64_validate(&outFile->elf, fileData, fileSize) != 0)
    {
        LOG_ERR("failed to validate ELF file '%s' while reading module metadata\n", filename);
        free(fileData);
        return ERR(MODULE, INVALELF);
    }

    Elf64_Shdr* moduleInfoShdr = elf64_get_section_by_name(&outFile->elf, MODULE_INFO_SECTION);
    if (moduleInfoShdr == NULL || moduleInfoShdr->sh_size < MODULE_MIN_INFO ||
        moduleInfoShdr->sh_size > MODULE_MAX_INFO)
    {
        LOG_ERR("failed to find valid module info section in ELF file '%s'\n", filename);
        free(fileData);
        return ERR(MODULE, INVALELF);
    }

    status =
        module_info_parse(&outFile->info, (const char*)((uintptr_t)outFile->elf.header + moduleInfoShdr->sh_offset));
    if (outFile->info == NULL)
    {
        free(fileData);
        return status;
    }

    return OK;
}

static void module_file_deinit(module_file_t* file)
{
    free(file->elf.header);
    free(file->info);
}

static status_t module_cache_symbols_add(module_file_t* file, const char* path)
{
    uint64_t index = 0;
    while (true)
    {
        Elf64_Sym* sym = elf64_get_symbol_by_index(&file->elf, index++);
        if (sym == NULL)
        {
            break;
        }

        if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_ABS)
        {
            continue;
        }

        Elf64_Symbol_Binding binding = ELF64_ST_BIND(sym->st_info);
        Elf64_Symbol_Type type = ELF64_ST_TYPE(sym->st_info);
        const char* symName = elf64_get_symbol_name(&file->elf, sym);
        if (!MODULE_SYMBOL_ALLOWED(type, binding, symName))
        {
            continue;
        }

        module_cached_symbol_t* symbolEntry = malloc(sizeof(module_cached_symbol_t));
        if (symbolEntry == NULL)
        {
            return ERR(MODULE, NOMEM);
        }
        map_entry_init(&symbolEntry->mapEntry);
        symbolEntry->name = strdup(symName);
        symbolEntry->modulePath = strdup(path);
        if (symbolEntry->name == NULL || symbolEntry->modulePath == NULL)
        {
            free(symbolEntry->name);
            free(symbolEntry->modulePath);
            free(symbolEntry);
            return ERR(MODULE, NOMEM);
        }

        uint64_t hash = hash_string(symName);
        if (map_find(&symbolCache, symName, hash) != NULL)
        {
            LOG_ERR("symbol name collision for '%s' in module '%s'\n", symName, path);
            free(symbolEntry->name);
            free(symbolEntry->modulePath);
            free(symbolEntry);
            return ERR(MODULE, EXIST);
        }
        map_insert(&symbolCache, &symbolEntry->mapEntry, hash);
    }

    return OK;
}

static status_t module_cache_device_types_add(module_file_t* file, const char* path)
{
    const char* ptr = file->info->deviceTypes;
    while (*ptr != '\0')
    {
        char deviceType[MODULE_MAX_DEVICE_STRING] = {0};
        uint64_t parsed = module_string_copy(ptr, deviceType, MODULE_MAX_DEVICE_STRING);
        if (parsed == 0)
        {
            break;
        }
        ptr += parsed + 1;

        uint64_t hash = hash_string(deviceType);
        map_entry_t* entry = map_find(&deviceCache, deviceType, hash);
        module_cached_device_t* cachedDevice = entry ? CONTAINER_OF(entry, module_cached_device_t, mapEntry) : NULL;

        if (cachedDevice == NULL)
        {
            cachedDevice = malloc(sizeof(module_cached_device_t));
            if (cachedDevice == NULL)
            {
                return ERR(MODULE, NOMEM);
            }
            map_entry_init(&cachedDevice->mapEntry);
            cachedDevice->type = strdup(deviceType);
            if (cachedDevice->type == NULL)
            {
                free(cachedDevice);
                return ERR(MODULE, NOMEM);
            }
            list_init(&cachedDevice->entries);

            map_insert(&deviceCache, &cachedDevice->mapEntry, hash);
        }

        module_cached_device_entry_t* deviceEntry = malloc(sizeof(module_cached_device_entry_t));
        if (deviceEntry == NULL)
        {
            return ERR(MODULE, NOMEM);
        }
        list_entry_init(&deviceEntry->listEntry);
        strncpy_s(deviceEntry->path, MAX_PATH, path, MAX_PATH);

        list_push_back(&cachedDevice->entries, &deviceEntry->listEntry);
    }

    return OK;
}

static module_cached_symbol_t* module_cache_lookup_symbol(const char* name)
{
    map_entry_t* entry = map_find(&symbolCache, name, hash_string(name));
    return entry ? CONTAINER_OF(entry, module_cached_symbol_t, mapEntry) : NULL;
}

static module_cached_device_t* module_cache_lookup_device_type(const char* type)
{
    map_entry_t* entry = map_find(&deviceCache, type, hash_string(type));
    return entry ? CONTAINER_OF(entry, module_cached_device_t, mapEntry) : NULL;
}

static void module_cache_clear(void)
{
    module_cached_symbol_t* cachedSymbol;
    module_cached_symbol_t* temp1;
    MAP_FOR_EACH_SAFE(cachedSymbol, temp1, &symbolCache, mapEntry)
    {
        free(cachedSymbol->modulePath);
        free(cachedSymbol->name);
        free(cachedSymbol);
        map_remove(&symbolCache, &cachedSymbol->mapEntry, hash_string(cachedSymbol->name));
    }

    module_cached_device_t* cachedDevice;
    module_cached_device_t* temp2;
    MAP_FOR_EACH_SAFE(cachedDevice, temp2, &deviceCache, mapEntry)
    {
        module_cached_device_entry_t* deviceEntry;
        while (!list_is_empty(&cachedDevice->entries))
        {
            deviceEntry = CONTAINER_OF(list_pop_front(&cachedDevice->entries), module_cached_device_entry_t, listEntry);
            free(deviceEntry);
        }
        free(cachedDevice->type);
        free(cachedDevice);
        map_remove(&deviceCache, &cachedDevice->mapEntry, hash_string(cachedDevice->type));
    }

    cacheValid = false;
}

static status_t module_cache_build(void)
{
    if (cacheValid)
    {
        return OK;
    }

    process_t* process = process_current();
    assert(process != NULL);

    pathname_t moduleDir;
    status_t status = pathname_init(&moduleDir, MODULE_DIR);
    if (IS_ERR(status))
    {
        return status;
    }

    file_t* dir;
    status = vfs_open(&dir, &moduleDir, process);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(dir);

    dirent_t buffer[PAGE_SIZE / sizeof(dirent_t)];
    while (true)
    {
        size_t bytesRead;
        status = vfs_getdents(dir, buffer, sizeof(buffer), &bytesRead);
        if (IS_ERR(status))
        {
            module_cache_clear();
            return status;
        }
        if (bytesRead == 0)
        {
            break;
        }

        for (uint64_t i = 0; i < bytesRead / sizeof(dirent_t); i++)
        {
            if (buffer[i].path[0] == '.' || buffer[i].type != VREG)
            {
                continue;
            }

            module_file_t file;
            status = module_file_read(&file, &dir->path, process, buffer[i].path);
            if (IS_ERR(status))
            {
                LOG_ERR("skipping invalid module file '%s' (%s)\n", buffer[i].path, codetostr(status));
                continue;
            }

            status = module_cache_symbols_add(&file, buffer[i].path);
            if (IS_ERR(status))
            {
                module_file_deinit(&file);
                module_cache_clear();
                return status;
            }

            status = module_cache_device_types_add(&file, buffer[i].path);
            if (IS_ERR(status))
            {
                module_file_deinit(&file);
                module_cache_clear();
                return status;
            }

            LOG_DEBUG("built cache entry for module '%s'\n", file.info->name);
            module_file_deinit(&file);
        }
    }

    cacheValid = true;
    return OK;
}

static void module_gc_mark_reachable(module_t* module)
{
    if (module == NULL || module->flags & MODULE_FLAG_GC_REACHABLE)
    {
        return;
    }

    module->flags |= MODULE_FLAG_GC_REACHABLE;
    module_dependency_t* dependency;
    LIST_FOR_EACH(dependency, &module->dependencies, listEntry)
    {
        module_gc_mark_reachable(dependency->module);
    }
}

// This makes sure we collect unreachable modulesList in dependency order
static void module_gc_sweep_unreachable(module_t* module, list_t* unreachables)
{
    if (module == NULL || module->flags & MODULE_FLAG_GC_REACHABLE || module->flags & MODULE_FLAG_GC_PINNED)
    {
        return;
    }
    module->flags |= MODULE_FLAG_GC_REACHABLE; // Prevent re-entrance

    list_push_back(unreachables, &module->gcEntry);

    module_dependency_t* dependency;
    LIST_FOR_EACH(dependency, &module->dependencies, listEntry)
    {
        module_gc_sweep_unreachable(dependency->module, unreachables);
    }
}

static void module_gc_collect(void)
{
    module_t* module;
    LIST_FOR_EACH(module, &modulesList, listEntry)
    {
        if (!list_is_empty(&module->deviceHandlers))
        {
            module_gc_mark_reachable(module);
        }
    }

    list_t unreachables = LIST_CREATE(unreachables);
    LIST_FOR_EACH(module, &modulesList, listEntry)
    {
        module_gc_sweep_unreachable(module, &unreachables);
    }

    LIST_FOR_EACH(module, &modulesList, listEntry)
    {
        module->flags &= ~MODULE_FLAG_GC_REACHABLE;
    }

    LIST_FOR_EACH(module, &unreachables, gcEntry)
    {
        // Be extra safe and call unload event before freeing any resources
        module_call_unload_event(module);
    }

    while (!list_is_empty(&unreachables))
    {
        module = CONTAINER_OF(list_pop_front(&unreachables), module_t, gcEntry);
        module_free(module);
    }
}

status_t module_init_fake_kernel_module(void)
{
    boot_info_t* bootInfo = boot_info_get();
    const boot_kernel_t* kernel = &bootInfo->kernel;
    const Elf64_File* elf = &kernel->elf;

    module_t* kernelModule = module_new(&fakeKernelModuleInfo);
    if (kernelModule == NULL)
    {
        return ERR(MODULE, NOMEM);
    }
    kernelModule->flags |= MODULE_FLAG_LOADED | MODULE_FLAG_GC_PINNED;

    uint64_t index = 0;
    while (true)
    {
        Elf64_Sym* sym = elf64_get_symbol_by_index(elf, index++);
        if (sym == NULL)
        {
            break;
        }

        const char* symName = elf64_get_symbol_name(elf, sym);
        void* symAddr = (void*)sym->st_value;
        Elf64_Symbol_Binding binding = ELF64_ST_BIND(sym->st_info);
        Elf64_Symbol_Type type = ELF64_ST_TYPE(sym->st_info);
        status_t status = symbol_add(symName, symAddr, kernelModule->symbolGroupId, binding, type);
        if (IS_ERR(status))
        {
            module_free(kernelModule);
            return status;
        }
    }

    LOG_INFO("loaded %llu kernel symbols\n", index);
    return OK;
}

static bool module_info_supports_device(const module_info_t* info, const char* type)
{
    if (info == NULL || type == NULL)
    {
        return false;
    }

    const char* deviceTypePtr = info->deviceTypes;
    while (*deviceTypePtr != '\0')
    {
        char currentDeviceType[MODULE_MAX_DEVICE_STRING] = {0};
        uint64_t parsed = module_string_copy(deviceTypePtr, currentDeviceType, MODULE_MAX_DEVICE_STRING);
        if (parsed == 0)
        {
            break;
        }
        deviceTypePtr += parsed + 1;

        if (strncmp(currentDeviceType, type, MODULE_MAX_DEVICE_STRING) == 0)
        {
            return true;
        }
    }

    return false;
}

static status_t module_load_and_relocate_elf(module_t* module, Elf64_File* elf, module_load_ctx_t* ctx)
{
    Elf64_Addr minVaddr;
    Elf64_Addr maxVaddr;
    elf64_get_loadable_bounds(elf, &minVaddr, &maxVaddr);
    uint64_t moduleMemSize = maxVaddr - minVaddr;

    // Will be unmapped in module_free
    module->baseAddr = NULL;
    status_t status = vmm_alloc(NULL, &module->baseAddr, moduleMemSize, PAGE_SIZE, PML_PRESENT | PML_WRITE | PML_GLOBAL,
        VMM_ALLOC_OVERWRITE);
    if (IS_ERR(status))
    {
        return status;
    }
    module->size = moduleMemSize;
    module->procedure = (module_procedure_t)((uintptr_t)module->baseAddr + (elf->header->e_entry - minVaddr));

    elf64_load_segments(elf, (Elf64_Addr)module->baseAddr, minVaddr);

    uint64_t index = 0;
    while (true)
    {
        Elf64_Sym* sym = elf64_get_symbol_by_index(elf, index++);
        if (sym == NULL)
        {
            break;
        }

        if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_ABS)
        {
            continue;
        }

        const char* symName = elf64_get_symbol_name(elf, sym);
        Elf64_Symbol_Binding binding = ELF64_ST_BIND(sym->st_info);
        Elf64_Symbol_Type type = ELF64_ST_TYPE(sym->st_info);
        if (!MODULE_SYMBOL_ALLOWED(type, binding, symName))
        {
            continue;
        }

        void* symAddr = (void*)((uintptr_t)module->baseAddr + (sym->st_value - minVaddr));
        status_t status = symbol_add(symName, symAddr, module->symbolGroupId, binding, type);
        if (IS_ERR(status))
        {
            LOG_ERR("failed to add symbol '%s' to module '%s'\n", symName, module->info.name);
            return status;
        }
    }

    module_t* previous = ctx->current;
    ctx->current = module;
    if (!elf64_relocate(elf, (Elf64_Addr)module->baseAddr, minVaddr, module_resolve_symbol_callback, ctx))
    {
        return ERR(MODULE, INVALELF);
    }
    ctx->current = previous;

    return OK;
}

static status_t module_load_dependency(module_load_ctx_t* ctx, const char* symbolName)
{
    map_entry_t* entry = map_find(&symbolCache, symbolName, hash_string(symbolName));
    module_cached_symbol_t* cacheEntry = entry ? CONTAINER_OF(entry, module_cached_symbol_t, mapEntry) : NULL;
    if (cacheEntry == NULL)
    {
        LOG_ERR("no cached module found for symbol '%s'\n", symbolName);
        return ERR(MODULE, NOENT);
    }

    module_file_t file;
    status_t status = module_file_read(&file, &ctx->dir->path, ctx->process, cacheEntry->modulePath);
    if (IS_ERR(status))
    {
        return status;
    }

    if (module_find_by_name(file.info->name) != NULL)
    {
        module_file_deinit(&file);
        return OK;
    }

    module_t* dependency = module_new(file.info);
    if (dependency == NULL)
    {
        module_file_deinit(&file);
        return ERR(MODULE, NOMEM);
    }

    LOG_INFO("loading dependency '%s' version %s by %s\n", dependency->info.name, dependency->info.version,
        dependency->info.author);
    LOG_DEBUG("  description: %s\n  licence:     %s\n", dependency->info.description, dependency->info.license);

    status = module_load_and_relocate_elf(dependency, &file.elf, ctx);
    if (IS_ERR(status))
    {
        module_file_deinit(&file);
        module_free(dependency);
        return status;
    }

    list_push_back(&ctx->dependencies, &dependency->loadEntry);
    module_file_deinit(&file);
    return OK;
}

static void* module_resolve_symbol_callback(const char* symbolName, void* data)
{
    module_load_ctx_t* ctx = data;

    symbol_info_t symbolInfo;
    if (IS_ERR(symbol_resolve_name(&symbolInfo, symbolName)))
    {
        if (IS_ERR(module_load_dependency(ctx, symbolName)))
        {
            return NULL;
        }

        if (IS_ERR(symbol_resolve_name(&symbolInfo, symbolName)))
        {
            LOG_ERR("failed to resolve symbol '%s' after loading dependency\n", symbolName);
            return NULL;
        }
    }

    module_t* existingModule = module_find_provider(symbolInfo.groupId);
    if (existingModule != NULL)
    {
        return symbolInfo.addr;
    }

    module_dependency_t* dependency = malloc(sizeof(module_dependency_t));
    if (dependency == NULL)
    {
        return NULL;
    }
    list_entry_init(&dependency->listEntry);
    dependency->module = existingModule;
    if (dependency->module == NULL)
    {
        free(dependency);
        return NULL;
    }

    module_dependency_t* existingDependency;
    LIST_FOR_EACH(existingDependency, &ctx->current->dependencies, listEntry)
    {
        if (existingDependency->module == dependency->module)
        {
            free(dependency);
            return symbolInfo.addr;
        }
    }

    list_push_back(&ctx->current->dependencies, &dependency->listEntry);
    return symbolInfo.addr;
}

static module_t* module_get_or_load(const char* filename, file_t* dir, const char* type)
{
    if (filename == NULL || dir == NULL || type == NULL)
    {
        return NULL;
    }

    module_load_ctx_t ctx = {
        .dir = dir,
        .filename = filename,
        .process = process_current(),
        .current = NULL,
        .dependencies = LIST_CREATE(ctx.dependencies),
    };

    module_file_t file;
    if (IS_ERR(module_file_read(&file, &ctx.dir->path, ctx.process, filename)))
    {
        return NULL;
    }

    if (!module_info_supports_device(file.info, type))
    {
        module_file_deinit(&file);
        return NULL;
    }

    module_t* existingModule = module_find_by_name(file.info->name);
    if (existingModule != NULL)
    {
        module_file_deinit(&file);
        return existingModule;
    }

    module_t* module = module_new(file.info);
    if (module == NULL)
    {
        module_file_deinit(&file);
        return NULL;
    }

    list_t loadedDependencies = LIST_CREATE(loadedDependencies);

    LOG_INFO("loading '%s' version %s by %s\n", module->info.name, module->info.version, module->info.author);
    LOG_DEBUG("  description: %s\n  licence:     %s\n", module->info.description, module->info.license);

    status_t status = module_load_and_relocate_elf(module, &file.elf, &ctx);
    module_file_deinit(&file);
    if (IS_ERR(status))
    {
        goto error;
    }

    while (!list_is_empty(&ctx.dependencies))
    {
        // Go in reverse to start at the deepest dependency
        module_t* dependency = CONTAINER_OF_SAFE(list_pop_back(&ctx.dependencies), module_t, loadEntry);
        if (dependency == NULL)
        {
            break;
        }

        if (IS_ERR(module_call_load_event(dependency)))
        {
            module_free(dependency);
            goto error;
        }

        list_push_back(&loadedDependencies, &dependency->loadEntry);
    }

    while (!list_is_empty(&loadedDependencies))
    {
        module_t* dependency = CONTAINER_OF(list_pop_front(&loadedDependencies), module_t, loadEntry);
        LOG_DEBUG("finished loading dependency module '%s'\n", dependency->info.name);
    }

    if (IS_ERR(module_call_load_event(module)))
    {
        goto error;
    }

    LOG_DEBUG("finished loading module '%s'\n", module->info.name);

    return module;

error:
    while (!list_is_empty(&loadedDependencies))
    {
        module_t* dependency = CONTAINER_OF(list_pop_back(&loadedDependencies), module_t, loadEntry);
        module_call_unload_event(dependency);
        module_free(dependency);
    }
    while (!list_is_empty(&ctx.dependencies))
    {
        module_t* dependency = CONTAINER_OF(list_pop_front(&ctx.dependencies), module_t, loadEntry);
        module_free(dependency);
    }
    module_free(module);
    return NULL;
}

status_t module_device_attach(const char* type, const char* name, module_load_flags_t flags, uint64_t* loadedCount)
{
    if (type == NULL || name == NULL)
    {
        return ERR(MODULE, INVAL);
    }

    MUTEX_SCOPE(&lock);

    status_t status = module_cache_build();
    if (IS_ERR(status))
    {
        return status;
    }

    module_cached_device_t* cachedDevice = module_cache_lookup_device_type(type);
    if (cachedDevice == NULL) // No modules support this device type
    {
        return OK;
    }

    pathname_t moduleDir;
    status = pathname_init(&moduleDir, MODULE_DIR);
    if (IS_ERR(status))
    {
        return status;
    }

    file_t* dir;
    status = vfs_open(&dir, &moduleDir, process_current());
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(dir);

    module_device_t* device = module_device_get(name);
    if (device != NULL)
    {
        if (strncmp(device->type, type, MODULE_MAX_DEVICE_STRING) != 0)
        {
            LOG_ERR("device '%s' type mismatch (expected '%s', got '%s')\n", name, device->type, type);
            return ERR(MODULE, INVAL);
        }

        if (!(flags & MODULE_LOAD_ALL) && !list_is_empty(&device->handlers))
        {
            return OK;
        }
    }
    else
    {
        device = module_device_new(type, name);
        if (device == NULL)
        {
            return ERR(MODULE, NOMEM);
        }
    }

    list_t handlers = LIST_CREATE(handlers);
    uint64_t count = 0;

    status = OK;
    module_cached_device_entry_t* deviceEntry;
    LIST_FOR_EACH(deviceEntry, &cachedDevice->entries, listEntry)
    {
        module_t* module = module_get_or_load(deviceEntry->path, dir, type);
        if (module == NULL)
        {
            LOG_ERR("failed to load module '%s' for device '%s'\n", deviceEntry->path, name);
            continue;
        }

        module_device_handler_t* handler;
        status = module_handler_add(&handler, module, device);
        if (IS_ERR(status))
        {
            goto error;
        }
        list_push_back(&handlers, &handler->loadEntry);
        count++;

        if (!(flags & MODULE_LOAD_ALL))
        {
            break;
        }
    }

    while (!list_is_empty(&handlers))
    {
        module_device_handler_t* handler = CONTAINER_OF(list_pop_front(&handlers), module_device_handler_t, loadEntry);
        LOG_DEBUG("added handler with module '%s' and device '%s'\n", handler->module->info.name, name);
    }

    if (loadedCount)
    {
        *loadedCount = count;
    }
    return OK;
error:
    while (!list_is_empty(&handlers))
    {
        module_device_handler_t* handler = CONTAINER_OF(list_pop_front(&handlers), module_device_handler_t, loadEntry);
        module_handler_remove(handler);
    }
    if (list_is_empty(&device->handlers))
    {
        module_device_free(device);
    }
    module_gc_collect();
    return status;
}

void module_device_detach(const char* name)
{
    if (name == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&lock);

    module_device_t* device = module_device_get(name);
    if (device == NULL)
    {
        return;
    }

    while (!list_is_empty(&device->handlers))
    {
        module_device_handler_t* handler =
            CONTAINER_OF(list_first(&device->handlers), module_device_handler_t, deviceEntry);
        module_handler_remove(handler);
    }
    module_gc_collect();

    if (list_is_empty(&device->handlers))
    {
        module_device_free(device);
    }
}

bool module_device_types_contains(const char* deviceTypes, const char* type)
{
    if (deviceTypes == NULL || type == NULL)
    {
        return false;
    }

    size_t idLen = strlen(type);
    const char* pos = deviceTypes;

    while ((pos = strstr(pos, type)) != NULL)
    {
        bool isStart = (pos == deviceTypes) || (*(pos - 1) == ';');
        bool isEnd = (pos[idLen] == ';') || (pos[idLen] == '\0');

        if (isStart && isEnd)
        {
            return true;
        }

        pos++;
    }

    return false;
}
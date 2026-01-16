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
#include <kernel/utils/map.h>
#include <kernel/version.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/elf.h>
#include <sys/io.h>
#include <sys/list.h>

static module_info_t fakeKernelModuleInfo = {
    .name = "kernel",
    .author = "The PatchworkOS Authors",
    .description = "The PatchworkOS Kernel",
    .version = OS_VERSION,
    .license = "MIT",
    .osVersion = OS_VERSION,
    .deviceTypes = "",
};

static list_t modulesList = LIST_CREATE(modulesList);
static map_t modulesMap = MAP_CREATE(); ///< Key = module name, value = module_t*
static map_t providerMap =
    MAP_CREATE(); ///< Key = symbol_group_id_t, value = module_t*. Used to find which module provides which symbols.

static map_t deviceMap = MAP_CREATE(); ///< Key = device name, value = module_device_t*

static map_t symbolCache = MAP_CREATE(); ///< Key = symbol name, value = module_cached_symbol_t*
static map_t deviceCache = MAP_CREATE(); ///< Key = device type, value = module_cached_device_t*
static bool cacheValid = false;

static mutex_t lock = MUTEX_CREATE(lock);

static void* module_resolve_symbol_callback(const char* name, void* data);

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

    map_key_t moduleKey = map_key_string(info->name);
    if (map_insert(&modulesMap, &moduleKey, &module->mapEntry) == ERR)
    {
        list_remove(&module->listEntry);
        free(module);
        return NULL;
    }

    map_key_t providerKey = map_key_uint64(module->symbolGroupId);
    if (map_insert(&providerMap, &providerKey, &module->providerEntry) == ERR)
    {
        list_remove(&module->listEntry);
        map_remove(&modulesMap, &module->mapEntry);
        free(module);
        return NULL;
    }

    return module;
}

static void module_free(module_t* module)
{
    LOG_DEBUG("freeing resources for module '%s'\n", module->info.name);

    assert(!(module->flags & MODULE_FLAG_LOADED));

    list_remove(&module->listEntry);
    map_remove(&modulesMap, &module->mapEntry);
    map_remove(&providerMap, &module->providerEntry);

    symbol_remove_group(module->symbolGroupId);

    if (module->baseAddr != NULL)
    {
        vmm_unmap(NULL, module->baseAddr, module->size);
    }

    free(module);
}

static uint64_t module_call_load_event(module_t* module)
{
    LOG_DEBUG("calling load event for module '%s'\n", module->info.name);
    module_event_t loadEvent = {
        .type = MODULE_EVENT_LOAD,
    };
    if (module->procedure(&loadEvent) == ERR)
    {
        LOG_ERR("call to load event for module '%s' failed\n", module->info.name);
        return ERR;
    }
    module->flags |= MODULE_FLAG_LOADED;
    return 0;
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
    map_key_t moduleKey = map_key_string(name);
    return CONTAINER_OF_SAFE(map_get(&modulesMap, &moduleKey), module_t, mapEntry);
}

static module_t* module_find_provider(symbol_group_id_t groupId)
{
    map_key_t providerKey = map_key_uint64(groupId);
    return CONTAINER_OF_SAFE(map_get(&providerMap, &providerKey), module_t, providerEntry);
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

    map_key_t deviceKey = map_key_string(name);
    if (map_insert(&deviceMap, &deviceKey, &device->mapEntry) == ERR)
    {
        free(device);
        return NULL;
    }

    return device;
}

static void module_device_free(module_device_t* device)
{
    assert(list_is_empty(&device->handlers));

    map_remove(&deviceMap, &device->mapEntry);
    free(device);
}

static module_device_t* module_device_get(const char* name)
{
    map_key_t deviceKey = map_key_string(name);
    return CONTAINER_OF_SAFE(map_get(&deviceMap, &deviceKey), module_device_t, mapEntry);
}

static inline module_device_handler_t* module_handler_add(module_t* module, module_device_t* device)
{
    module_device_handler_t* handler = malloc(sizeof(module_device_handler_t));
    if (handler == NULL)
    {
        return NULL;
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
    if (module->procedure(&attachEvent) == ERR)
    {
        LOG_ERR("call to attach event for module '%s' failed\n", module->info.name);
        free(handler);
        return NULL;
    }

    list_push_back(&device->handlers, &handler->deviceEntry);
    list_push_back(&module->deviceHandlers, &handler->moduleEntry);
    return handler;
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

static inline module_info_t* module_info_parse(const char* moduleInfo)
{
    size_t totalSize = strnlen_s(moduleInfo, MODULE_MAX_INFO);
    if (totalSize < MODULE_MIN_INFO || totalSize >= MODULE_MAX_INFO)
    {
        LOG_ERR("module info string is of invalid size %zu\n", totalSize);
        errno = EILSEQ;
        return NULL;
    }

    module_info_t* info = malloc(sizeof(module_info_t) + totalSize + 1);
    if (info == NULL)
    {
        return NULL;
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
    return info;

error:
    free(info);
    errno = EILSEQ;
    return NULL;
}

typedef struct
{
    Elf64_File elf;
    module_info_t* info;
} module_file_t;

static uint64_t module_file_read(module_file_t* outFile, const path_t* dirPath, process_t* process,
    const char* filename)
{
    pathname_t pathname;
    if (pathname_init(&pathname, filename) == ERR)
    {
        return ERR;
    }

    file_t* file = vfs_openat(dirPath, &pathname, process);
    if (file == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(file);

    size_t fileSize = vfs_seek(file, 0, SEEK_END);
    vfs_seek(file, 0, SEEK_SET);
    if (fileSize == ERR)
    {
        return ERR;
    }

    uint8_t* fileData = malloc(fileSize);
    if (fileData == NULL)
    {
        return ERR;
    }

    if (vfs_read(file, fileData, fileSize) != fileSize)
    {
        free(fileData);
        return ERR;
    }

    if (elf64_validate(&outFile->elf, fileData, fileSize) == ERR)
    {
        LOG_ERR("failed to validate ELF file '%s' while reading module metadata\n", filename);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    Elf64_Shdr* moduleInfoShdr = elf64_get_section_by_name(&outFile->elf, MODULE_INFO_SECTION);
    if (moduleInfoShdr == NULL || moduleInfoShdr->sh_size < MODULE_MIN_INFO ||
        moduleInfoShdr->sh_size > MODULE_MAX_INFO)
    {
        LOG_ERR("failed to find valid module info section in ELF file '%s'\n", filename);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    outFile->info = module_info_parse((const char*)((uintptr_t)outFile->elf.header + moduleInfoShdr->sh_offset));
    if (outFile->info == NULL)
    {
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

static void module_file_deinit(module_file_t* file)
{
    free(file->elf.header);
    free(file->info);
}

static uint64_t module_cache_symbols_add(module_file_t* file, const char* path)
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
            return ERR;
        }
        map_entry_init(&symbolEntry->mapEntry);
        symbolEntry->modulePath = strdup(path);
        if (symbolEntry->modulePath == NULL)
        {
            free(symbolEntry);
            return ERR;
        }

        map_key_t symbolKey = map_key_string(symName);
        if (map_insert(&symbolCache, &symbolKey, &symbolEntry->mapEntry) == ERR)
        {
            if (errno == EEXIST)
            {
                LOG_ERR("symbol name collision for '%s' in module '%s'\n", symName, path);
            }
            free(symbolEntry);
            return ERR;
        }
    }

    return 0;
}

static uint64_t module_cache_device_types_add(module_file_t* file, const char* path)
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

        map_key_t key = map_key_string(deviceType);
        module_cached_device_t* cachedDevice =
            CONTAINER_OF_SAFE(map_get(&deviceCache, &key), module_cached_device_t, mapEntry);
        if (cachedDevice == NULL)
        {
            cachedDevice = malloc(sizeof(module_cached_device_t));
            if (cachedDevice == NULL)
            {
                return ERR;
            }
            map_entry_init(&cachedDevice->mapEntry);
            list_init(&cachedDevice->entries);

            if (map_insert(&deviceCache, &key, &cachedDevice->mapEntry) == ERR)
            {
                free(cachedDevice);
                return ERR;
            }
        }

        module_cached_device_entry_t* deviceEntry = malloc(sizeof(module_cached_device_entry_t));
        if (deviceEntry == NULL)
        {
            return ERR;
        }
        list_entry_init(&deviceEntry->listEntry);
        strncpy_s(deviceEntry->path, MAX_PATH, path, MAX_PATH);

        list_push_back(&cachedDevice->entries, &deviceEntry->listEntry);
    }

    return 0;
}

static module_cached_symbol_t* module_cache_lookup_symbol(const char* name)
{
    map_key_t symbolKey = map_key_string(name);
    return CONTAINER_OF_SAFE(map_get(&symbolCache, &symbolKey), module_cached_symbol_t, mapEntry);
}

static module_cached_device_t* module_cache_lookup_device_type(const char* type)
{
    map_key_t deviceKey = map_key_string(type);
    return CONTAINER_OF_SAFE(map_get(&deviceCache, &deviceKey), module_cached_device_t, mapEntry);
}

static void module_cache_clear(void)
{
    for (uint64_t i = 0; i < symbolCache.capacity; i++)
    {
        map_entry_t* entry = symbolCache.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_cached_symbol_t* cachedSymbol = CONTAINER_OF(entry, module_cached_symbol_t, mapEntry);
        free(cachedSymbol->modulePath);
        free(cachedSymbol);
    }
    map_clear(&symbolCache);

    for (uint64_t i = 0; i < deviceCache.capacity; i++)
    {
        map_entry_t* entry = deviceCache.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_cached_device_t* cachedDevice = CONTAINER_OF(entry, module_cached_device_t, mapEntry);
        module_cached_device_entry_t* deviceEntry;
        while (!list_is_empty(&cachedDevice->entries))
        {
            deviceEntry = CONTAINER_OF(list_pop_front(&cachedDevice->entries), module_cached_device_entry_t, listEntry);
            free(deviceEntry);
        }
        free(cachedDevice);
    }
    map_clear(&deviceCache);

    cacheValid = false;
}

static uint64_t module_cache_build(void)
{
    if (cacheValid)
    {
        return 0;
    }

    process_t* process = process_current();
    assert(process != NULL);

    pathname_t moduleDir;
    if (pathname_init(&moduleDir, MODULE_DIR) == ERR)
    {
        return ERR;
    }

    file_t* dir = vfs_open(&moduleDir, process);
    if (dir == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(dir);

    dirent_t buffer[PAGE_SIZE / sizeof(dirent_t)];
    while (true)
    {
        size_t readCount = vfs_getdents(dir, buffer, sizeof(buffer));
        if (readCount == ERR)
        {
            module_cache_clear();
            return ERR;
        }
        if (readCount == 0)
        {
            break;
        }

        for (uint64_t i = 0; i < readCount / sizeof(dirent_t); i++)
        {
            if (buffer[i].path[0] == '.' || buffer[i].type != INODE_FILE)
            {
                continue;
            }

            module_file_t file;
            if (module_file_read(&file, &dir->path, process, buffer[i].path) == ERR)
            {
                if (errno == EILSEQ)
                {
                    LOG_ERR("skipping invalid module file '%s'\n", buffer[i].path);
                    continue;
                }
                module_cache_clear();
                return ERR;
            }

            if (module_cache_symbols_add(&file, buffer[i].path) == ERR)
            {
                module_file_deinit(&file);
                module_cache_clear();
                return ERR;
            }

            if (module_cache_device_types_add(&file, buffer[i].path) == ERR)
            {
                module_file_deinit(&file);
                module_cache_clear();
                return ERR;
            }

            LOG_DEBUG("built cache entry for module '%s'\n", file.info->name);
            module_file_deinit(&file);
        }
    }

    cacheValid = true;
    return 0;
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

void module_init_fake_kernel_module(void)
{
    boot_info_t* bootInfo = boot_info_get();
    const boot_kernel_t* kernel = &bootInfo->kernel;
    const Elf64_File* elf = &kernel->elf;

    module_t* kernelModule = module_new(&fakeKernelModuleInfo);
    if (kernelModule == NULL)
    {
        panic(NULL, "Failed to create fake kernel module (%s)", strerror(errno));
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
        if (symbol_add(symName, symAddr, kernelModule->symbolGroupId, binding, type) == ERR)
        {
            panic(NULL, "Failed to load kernel symbol '%s' (%s)", symName, strerror(errno));
        }
    }

    LOG_INFO("loaded %llu kernel symbols\n", index);
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

typedef struct
{
    file_t* dir;
    const char* filename;
    process_t* process;
    module_t* current; ///< The module whose dependencies are currently being loaded.
    list_t dependencies;
} module_load_ctx_t;

static uint64_t module_load_and_relocate_elf(module_t* module, Elf64_File* elf, module_load_ctx_t* ctx)
{
    Elf64_Addr minVaddr;
    Elf64_Addr maxVaddr;
    elf64_get_loadable_bounds(elf, &minVaddr, &maxVaddr);
    uint64_t moduleMemSize = maxVaddr - minVaddr;

    // Will be unmapped in module_free
    module->baseAddr =
        vmm_alloc(NULL, NULL, moduleMemSize, PAGE_SIZE, PML_PRESENT | PML_WRITE | PML_GLOBAL, VMM_ALLOC_OVERWRITE);
    if (module->baseAddr == NULL)
    {
        return ERR;
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
        if (symbol_add(symName, symAddr, module->symbolGroupId, binding, type) == ERR)
        {
            LOG_ERR("failed to add symbol '%s' to module '%s'\n", symName, module->info.name);
            return ERR;
        }
    }

    module_t* previous = ctx->current;
    ctx->current = module;
    if (elf64_relocate(elf, (Elf64_Addr)module->baseAddr, minVaddr, module_resolve_symbol_callback, ctx) == ERR)
    {
        return ERR;
    }
    ctx->current = previous;

    return 0;
}

static uint64_t module_load_dependency(module_load_ctx_t* ctx, const char* symbolName)
{
    map_key_t cacheKey = map_key_string(symbolName);
    module_cached_symbol_t* cacheEntry =
        CONTAINER_OF_SAFE(map_get(&symbolCache, &cacheKey), module_cached_symbol_t, mapEntry);
    if (cacheEntry == NULL)
    {
        LOG_ERR("no cached module found for symbol '%s'\n", symbolName);
        return ERR;
    }

    module_file_t file;
    if (module_file_read(&file, &ctx->dir->path, ctx->process, cacheEntry->modulePath) == ERR)
    {
        return ERR;
    }

    if (module_find_by_name(file.info->name) != NULL)
    {
        module_file_deinit(&file);
        return 0;
    }

    module_t* dependency = module_new(file.info);
    if (dependency == NULL)
    {
        module_file_deinit(&file);
        return ERR;
    }

    LOG_INFO("loading dependency '%s' version %s by %s\n", dependency->info.name, dependency->info.version,
        dependency->info.author);
    LOG_DEBUG("  description: %s\n  licence:     %s\n", dependency->info.description, dependency->info.license);

    if (module_load_and_relocate_elf(dependency, &file.elf, ctx) == ERR)
    {
        module_file_deinit(&file);
        module_free(dependency);
        return ERR;
    }

    list_push_back(&ctx->dependencies, &dependency->loadEntry);
    module_file_deinit(&file);
    return 0;
}

static void* module_resolve_symbol_callback(const char* symbolName, void* data)
{
    module_load_ctx_t* ctx = data;

    symbol_info_t symbolInfo;
    if (symbol_resolve_name(&symbolInfo, symbolName) == ERR)
    {
        if (module_load_dependency(ctx, symbolName) == ERR)
        {
            return NULL;
        }

        if (symbol_resolve_name(&symbolInfo, symbolName) == ERR)
        {
            LOG_ERR("failed to resolve symbol '%s' after loading dependency\n", symbolName);
            return NULL;
        }
    }

    map_key_t providerKey = map_key_uint64(symbolInfo.groupId);
    module_t* existingModule = CONTAINER_OF_SAFE(map_get(&providerMap, &providerKey), module_t, providerEntry);
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
    dependency->module = CONTAINER_OF_SAFE(map_get(&providerMap, &providerKey), module_t, providerEntry);
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
        errno = EINVAL;
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
    if (module_file_read(&file, &ctx.dir->path, ctx.process, filename) == ERR)
    {
        return NULL;
    }

    if (!module_info_supports_device(file.info, type))
    {
        module_file_deinit(&file);
        errno = ENODEV;
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

    uint64_t loadResult = module_load_and_relocate_elf(module, &file.elf, &ctx);
    module_file_deinit(&file);
    if (loadResult == ERR)
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

        if (module_call_load_event(dependency) == ERR)
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

    if (module_call_load_event(module) == ERR)
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

uint64_t module_device_attach(const char* type, const char* name, module_load_flags_t flags)
{
    if (type == NULL || name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&lock);

    if (module_cache_build() == ERR)
    {
        return ERR;
    }

    module_cached_device_t* cachedDevice = module_cache_lookup_device_type(type);
    if (cachedDevice == NULL) // No modules support this device type
    {
        return 0;
    }

    pathname_t moduleDir;
    if (pathname_init(&moduleDir, MODULE_DIR) == ERR)
    {
        return ERR;
    }

    file_t* dir = vfs_open(&moduleDir, process_current());
    if (dir == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(dir);

    module_device_t* device = module_device_get(name);
    if (device != NULL)
    {
        if (strncmp(device->type, type, MODULE_MAX_DEVICE_STRING) != 0)
        {
            LOG_ERR("device '%s' type mismatch (expected '%s', got '%s')\n", name, device->type, type);
            errno = EINVAL;
            return ERR;
        }

        if (!(flags & MODULE_LOAD_ALL) && !list_is_empty(&device->handlers))
        {
            return 0;
        }
    }
    else
    {
        device = module_device_new(type, name);
        if (device == NULL)
        {
            return ERR;
        }
    }

    list_t handlers = LIST_CREATE(handlers);
    uint64_t loadedCount = 0;

    module_cached_device_entry_t* deviceEntry;
    LIST_FOR_EACH(deviceEntry, &cachedDevice->entries, listEntry)
    {
        module_t* module = module_get_or_load(deviceEntry->path, dir, type);
        if (module == NULL)
        {
            LOG_ERR("failed to load module '%s' for device '%s'\n", deviceEntry->path, name);
            continue;
        }

        module_device_handler_t* handler = module_handler_add(module, device);
        if (handler == NULL)
        {
            goto error;
        }
        list_push_back(&handlers, &handler->loadEntry);
        loadedCount++;

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

    return loadedCount;
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
    return ERR;
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
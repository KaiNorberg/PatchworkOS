#include <_internal/MAX_PATH.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/sysfs.h>
#include <kernel/module/module.h>

#include <kernel/fs/vfs.h>
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
    .deviceIds = "",
};

// Key = module symbol group ID, value = module_t*
static map_t dependencyMap = MAP_CREATE;

// Key = module name, value = module_t*
static map_t moduleMap = MAP_CREATE;

// Key = device ID, value = module_device_t*
static map_t deviceMap = MAP_CREATE;

// Key = symbol name, value = module_cached_symbol_t*
static map_t symbolCache = MAP_CREATE;

// Key = device ID, value = module_cached_device_t*
static map_t deviceIdCache = MAP_CREATE;

static bool cacheValid = false;

static mutex_t lock = MUTEX_CREATE(lock);

#define MODULE_SYMBOL_ALLOWED(type, binding, name) \
    ((type == STT_OBJECT || type == STT_FUNC) && (binding == STB_GLOBAL) && \
        (strncmp(name, MODULE_RESERVED_PREFIX, MODULE_RESERVED_PREFIX_LENGTH) != 0))

static inline uint64_t module_parse_string(const char* str, char* out, size_t outSize)
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

static module_t* module_new(module_info_t* info)
{
    module_t* module = malloc(sizeof(module_t));
    if (module == NULL)
    {
        return NULL;
    }
    map_entry_init(&module->dependencyMapEntry);
    map_entry_init(&module->moduleMapEntry);
    list_entry_init(&module->entry);
    module->flags = MODULE_FLAG_NONE;
    module->baseAddr = NULL;
    module->size = 0;
    module->procedure = NULL;
    module->symbolGroupId = symbol_generate_group_id();
    map_init(&module->deviceHandlers);
    map_init(&module->dependencies);
    module->info = info;

    map_key_t dependKey = map_key_uint64(module->symbolGroupId);
    if (map_insert(&dependencyMap, &dependKey, &module->dependencyMapEntry) == ERR)
    {
        LOG_ERR("failed to insert module '%s' into dependency map\n", info->name);
        free(module);
        return NULL;
    }

    map_key_t moduleKey = map_key_string(module->info->name);
    if (map_insert(&moduleMap, &moduleKey, &module->moduleMapEntry) == ERR)
    {
        LOG_ERR("failed to insert module '%s' into module map\n", info->name);
        free(module);
        map_remove(&dependencyMap, &module->dependencyMapEntry);
        return NULL;
    }

    return module;
}

static uint64_t module_call_load_event(module_t* module)
{
    LOG_DEBUG("calling load event for module '%s'\n", module->info->name);
    module_event_t loadEvent = {
        .type = MODULE_EVENT_LOAD,
    };
    if (module->procedure(&loadEvent) == ERR)
    {
        LOG_ERR("call to load event for module '%s' failed\n", module->info->name);
        return ERR;
    }
    module->flags |= MODULE_FLAG_LOADED;
    return 0;
}

static void module_call_unload_event(module_t* module)
{
    if (module->flags & MODULE_FLAG_LOADED)
    {
        LOG_DEBUG("calling unload event for module '%s'\n", module->info->name);
        module_event_t unloadEvent = {
            .type = MODULE_EVENT_UNLOAD,
        };
        module->procedure(&unloadEvent);
        module->flags &= ~MODULE_FLAG_LOADED;
    }
}

static void module_free(module_t* module)
{
    LOG_DEBUG("freeing resources for module '%s'\n", module->info->name);

    map_remove(&dependencyMap, &module->dependencyMapEntry);
    map_remove(&moduleMap, &module->moduleMapEntry);

    symbol_remove_group(module->symbolGroupId);

    if (module->baseAddr != NULL)
    {
        vmm_unmap(NULL, module->baseAddr, module->size);
    }

    for (uint64_t i = 0; i < module->dependencies.capacity; i++)
    {
        map_entry_t* entry = module->dependencies.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_dependency_t* dependency = CONTAINER_OF(entry, module_dependency_t, entry);
        free(dependency);
    }
    map_deinit(&module->dependencies);

    free(module->info);
    free(module);
}

static inline uint64_t module_device_attach(module_device_t* device, module_t* module)
{
    module_device_handler_t* handler = malloc(sizeof(module_device_handler_t));
    if (handler == NULL)
    {
        return ERR;
    }
    list_entry_init(&handler->deviceEntry);
    map_entry_init(&handler->moduleEntry);
    handler->module = module;
    handler->device = device;

    map_key_t deviceKey = map_key_string(device->id);
    if (map_insert(&module->deviceHandlers, &deviceKey, &handler->moduleEntry) == ERR)
    {
        free(handler);
        return ERR;
    }

    module_event_t attachEvent = {
        .type = MODULE_EVENT_DEVICE_ATTACH,
        .deviceAttach.id = device->id,
    };
    if (module->procedure(&attachEvent) == ERR)
    {
        LOG_ERR("call to attach event for module '%s' failed\n", module->info->name);
        map_remove(&module->deviceHandlers, &handler->moduleEntry);
        free(handler);
        return ERR;
    }

    list_push_back(&device->handlers, &handler->deviceEntry);
    return 0;
}

static inline uint64_t module_device_detach(module_device_t* device, module_t* module)
{
    map_key_t deviceKey = map_key_string(device->id);
    map_entry_t* entry = map_get_and_remove(&module->deviceHandlers, &deviceKey);
    if (entry == NULL)
    {
        LOG_ERR("failed to detach device '%s' from module '%s'\n", device->id, module->info->name);
        return ERR;
    }

    module_device_handler_t* handler = CONTAINER_OF(entry, module_device_handler_t, moduleEntry);

    module_event_t detachEvent = {
        .type = MODULE_EVENT_DEVICE_DETACH,
        .deviceDetach.id = device->id,
    };
    module->procedure(&detachEvent);

    list_remove(&device->handlers, &handler->deviceEntry);
    free(handler);

    if (list_is_empty(&device->handlers))
    {
        map_remove(&deviceMap, &device->mapEntry);
        free(device);
    }
    return 0;
}

void module_init_fake_kernel_module(const boot_kernel_t* kernel)
{
    module_t* kernelModule = module_new(&fakeKernelModuleInfo);
    if (kernelModule == NULL)
    {
        panic(NULL, "Failed to create fake kernel module (%s)", strerror(errno));
    }
    kernelModule->flags |= MODULE_FLAG_LOADED | MODULE_FLAG_GC_PINNED;

    const Elf64_File* elf = &kernel->elf;
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

static void module_gc_mark_reachable(module_t* module)
{
    if (module == NULL || module->flags & MODULE_FLAG_GC_REACHABLE)
    {
        return;
    }

    module->flags |= MODULE_FLAG_GC_REACHABLE;
    for (uint64_t i = 0; i < module->dependencies.capacity; i++)
    {
        map_entry_t* entry = module->dependencies.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_dependency_t* dependency = CONTAINER_OF(entry, module_dependency_t, entry);
        module_gc_mark_reachable(dependency->module);
    }
}

// This makes sure we collect unreachable modules in dependency order
static void module_gc_sweep_unreachable(module_t* module, list_t* toFree)
{
    if (module == NULL || module->flags & MODULE_FLAG_GC_REACHABLE || module->flags & MODULE_FLAG_GC_PINNED)
    {
        return;
    }
    module->flags |= MODULE_FLAG_GC_REACHABLE; // Prevent re-entrance

    list_push_back(toFree, &module->entry);

    for (uint64_t i = 0; i < module->dependencies.capacity; i++)
    {
        map_entry_t* entry = module->dependencies.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_dependency_t* dependency = CONTAINER_OF(entry, module_dependency_t, entry);
        module_gc_sweep_unreachable(dependency->module, toFree);
    }
}

static void module_collect_garbage(void)
{
    for (uint64_t i = 0; i < moduleMap.capacity; i++)
    {
        map_entry_t* entry = moduleMap.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_t* module = CONTAINER_OF(entry, module_t, moduleMapEntry);
        if (!map_is_empty(&module->deviceHandlers))
        {
            module_gc_mark_reachable(module);
        }
    }

    list_t toFree = LIST_CREATE(toFree);
    for (uint64_t i = 0; i < moduleMap.capacity; i++)
    {
        map_entry_t* entry = moduleMap.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_t* module = CONTAINER_OF(entry, module_t, moduleMapEntry);
        module_gc_sweep_unreachable(module, &toFree);
    }

    for (uint64_t i = 0; i < moduleMap.capacity; i++)
    {
        map_entry_t* entry = moduleMap.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_t* module = CONTAINER_OF(entry, module_t, moduleMapEntry);
        module->flags &= ~MODULE_FLAG_GC_REACHABLE;
    }

    module_t* module;
    LIST_FOR_EACH(module, &toFree, entry)
    {
        // Be extra safe and call unload event before freeing any resources
        module_call_unload_event(module);
    }

    while (!list_is_empty(&toFree))
    {
        module = CONTAINER_OF(list_pop_first(&toFree), module_t, entry);
        module_free(module);
    }
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
    size_t parsed = module_parse_string(&moduleInfo[offset], info->name, MODULE_MAX_NAME);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module name\n");
        goto error;
    }
    info->name[parsed] = '\0';
    offset += parsed + 1;

    info->author = &info->data[offset];
    parsed = module_parse_string(&moduleInfo[offset], info->author, MODULE_MAX_AUTHOR);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module author\n");
        goto error;
    }
    info->author[parsed] = '\0';
    offset += parsed + 1;

    info->description = &info->data[offset];
    parsed = module_parse_string(&moduleInfo[offset], info->description, MODULE_MAX_DESCRIPTION);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module description\n");
        goto error;
    }
    info->description[parsed] = '\0';
    offset += parsed + 1;

    info->version = &info->data[offset];
    parsed = module_parse_string(&moduleInfo[offset], info->version, MODULE_MAX_VERSION);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module version\n");
        goto error;
    }
    info->version[parsed] = '\0';
    offset += parsed + 1;

    info->license = &info->data[offset];
    parsed = module_parse_string(&moduleInfo[offset], info->license, MODULE_MAX_LICENSE);
    if (parsed == 0)
    {
        LOG_ERR("failed to parse module license\n");
        goto error;
    }
    info->license[parsed] = '\0';
    offset += parsed + 1;

    info->osVersion = &info->data[offset];
    parsed = module_parse_string(&moduleInfo[offset], info->osVersion, MODULE_MAX_VERSION);
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

    size_t deviceIdsLength = totalSize - offset;
    info->deviceIds = &info->data[offset];
    strncpy_s(info->deviceIds, deviceIdsLength + 1, &moduleInfo[offset], deviceIdsLength + 1);

    return info;

error:
    free(info);
    errno = EILSEQ;
    return NULL;
}

static bool module_info_has_device_id(module_info_t* info, const char* deviceId)
{
    if (info == NULL || deviceId == NULL)
    {
        return false;
    }

    const char* deviceIdPtr = info->deviceIds;
    while (*deviceIdPtr != '\0')
    {
        char currentDeviceId[MODULE_MAX_DEVICE_ID] = {0};
        uint64_t parsed = module_parse_string(deviceIdPtr, currentDeviceId, MODULE_MAX_DEVICE_ID);
        if (parsed == 0)
        {
            break;
        }
        deviceIdPtr += parsed + 1;

        if (strncmp(currentDeviceId, deviceId, MODULE_MAX_DEVICE_ID) == 0)
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
    module_t* parentModule; ///< The module whose dependencies are currently being loaded.
    list_t dependencies;
} module_load_ctx_t;

static void* module_resolve_callback(const char* name, void* private);

static uint64_t module_read_metadata(Elf64_File* outFile, module_info_t** outInfo, const path_t* dirPath,
    process_t* process, const char* filename)
{
    file_t* file = vfs_openat(dirPath, PATHNAME(filename), process);
    if (file == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(file);

    uint64_t fileSize = vfs_seek(file, 0, SEEK_END);
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

    if (vfs_read(file, fileData, fileSize) == ERR)
    {
        free(fileData);
        return ERR;
    }

    if (elf64_validate(outFile, fileData, fileSize) == ERR)
    {
        LOG_ERR("failed to validate ELF file '%s' while reading module metadata\n", filename);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    Elf64_Shdr* moduleInfoShdr = elf64_get_section_by_name(outFile, MODULE_INFO_SECTION);
    if (moduleInfoShdr == NULL || moduleInfoShdr->sh_size < MODULE_MIN_INFO ||
        moduleInfoShdr->sh_size > MODULE_MAX_INFO)
    {
        LOG_ERR("failed to find valid module info section in ELF file '%s'\n", filename);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    *outInfo = module_info_parse((const char*)((uintptr_t)outFile->header + moduleInfoShdr->sh_offset));
    if (*outInfo == NULL)
    {
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    return 0; // Caller owns fileData in outFile->header and outInfo
}

static uint64_t module_load_and_relocate_elf(module_t* module, Elf64_File* elf, module_load_ctx_t* ctx)
{
    Elf64_Addr minVaddr;
    Elf64_Addr maxVaddr;
    elf64_get_loadable_bounds(elf, &minVaddr, &maxVaddr);
    uint64_t moduleMemSize = maxVaddr - minVaddr;

    // Will be unmapped in module_free
    module->baseAddr = vmm_alloc(NULL, NULL, moduleMemSize, PML_PRESENT | PML_WRITE | PML_GLOBAL, VMM_ALLOC_NONE);
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
            LOG_ERR("failed to add symbol '%s' to module '%s'\n", symName, module->info->name);
            return ERR;
        }
    }

    module_t* previousParent = ctx->parentModule;
    ctx->parentModule = module;
    if (elf64_relocate(elf, (Elf64_Addr)module->baseAddr, minVaddr, module_resolve_callback, ctx) == ERR)
    {
        return ERR;
    }
    ctx->parentModule = previousParent;

    return 0;
}

static uint64_t module_load_dependency_for_symbol(module_load_ctx_t* ctx, const char* symbolName)
{
    map_key_t cacheKey = map_key_string(symbolName);
    module_cached_symbol_t* cacheEntry =
        CONTAINER_OF_SAFE(map_get(&symbolCache, &cacheKey), module_cached_symbol_t, mapEntry);
    if (cacheEntry == NULL)
    {
        LOG_ERR("no cached module found for symbol '%s'\n", symbolName);
        return ERR;
    }

    Elf64_File elf;
    module_info_t* info;
    if (module_read_metadata(&elf, &info, &ctx->dir->path, ctx->process, cacheEntry->modulePath) == ERR)
    {
        return ERR;
    }

    map_key_t moduleKey = map_key_string(info->name);
    module_t* existingModule = CONTAINER_OF_SAFE(map_get(&moduleMap, &moduleKey), module_t, moduleMapEntry);
    if (existingModule != NULL)
    {
        free(elf.header);
        free(info);
        return 0; // Already loaded
    }

    module_t* dependency = module_new(info);
    if (dependency == NULL)
    {
        free(elf.header);
        free(info);
        return ERR;
    }

    LOG_INFO("loading dependency '%s' version %s by %s\n", dependency->info->name, dependency->info->version,
        dependency->info->author);
    LOG_DEBUG("  description: %s\n  licence:     %s\n", dependency->info->description, dependency->info->license);

    list_push_back(&ctx->dependencies, &dependency->entry);

    if (module_load_and_relocate_elf(dependency, &elf, ctx) == ERR)
    {
        free(elf.header);
        list_remove(&ctx->dependencies, &dependency->entry);
        module_free(dependency);
        return ERR;
    }

    free(elf.header);
    return 0;
}

static void* module_resolve_callback(const char* name, void* private)
{
    module_load_ctx_t* ctx = private;

    symbol_info_t symbolInfo;
    if (symbol_resolve_name(&symbolInfo, name) == ERR)
    {
        if (module_load_dependency_for_symbol(ctx, name) == ERR)
        {
            return NULL;
        }

        if (symbol_resolve_name(&symbolInfo, name) == ERR)
        {
            LOG_ERR("failed to resolve symbol '%s' after loading dependency\n", name);
            return NULL;
        }
    }

    map_key_t dependKey = map_key_uint64(symbolInfo.groupId);
    module_t* existing =
        CONTAINER_OF_SAFE(map_get(&ctx->parentModule->dependencies, &dependKey), module_t, dependencyMapEntry);
    if (existing != NULL)
    {
        return symbolInfo.addr;
    }

    module_dependency_t* dependency = malloc(sizeof(module_dependency_t));
    if (dependency == NULL)
    {
        return NULL;
    }
    map_entry_init(&dependency->entry);
    dependency->module = CONTAINER_OF_SAFE(map_get(&dependencyMap, &dependKey), module_t, dependencyMapEntry);
    if (dependency->module == NULL)
    {
        free(dependency);
        return NULL;
    }

    if (map_insert(&ctx->parentModule->dependencies, &dependKey, &dependency->entry) == ERR)
    {
        free(dependency);
        return NULL;
    }

    return symbolInfo.addr;
}

static module_t* module_get_or_load_filename(file_t* dir, const char* filename, const char* deviceId)
{
    if (filename == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    module_load_ctx_t ctx = {
        .dir = dir,
        .filename = filename,
        .process = sched_process(),
        .dependencies = LIST_CREATE(ctx.dependencies),
    };

    Elf64_File elf;
    module_info_t* info;
    if (module_read_metadata(&elf, &info, &ctx.dir->path, ctx.process, filename) == ERR)
    {
        return NULL;
    }

    if (!module_info_has_device_id(info, deviceId))
    {
        free(elf.header);
        free(info);
        errno = ENODEV;
        return NULL;
    }

    map_key_t moduleKey = map_key_string(info->name);
    module_t* existingModule = CONTAINER_OF_SAFE(map_get(&moduleMap, &moduleKey), module_t, moduleMapEntry);
    if (existingModule != NULL)
    {
        free(elf.header);
        free(info);
        return existingModule;
    }

    module_t* module = module_new(info);
    if (module == NULL)
    {
        free(elf.header);
        free(info);
        return NULL;
    }

    list_t loadedDependencies = LIST_CREATE(loadedDependencies);

    LOG_INFO("loading '%s' version %s by %s\n", module->info->name, module->info->version, module->info->author);
    LOG_DEBUG("  description: %s\n  licence:     %s\n", module->info->description, module->info->license);

    uint64_t loadResult = module_load_and_relocate_elf(module, &elf, &ctx);
    free(elf.header);
    if (loadResult == ERR)
    {
        goto error;
    }

    while (!list_is_empty(&ctx.dependencies))
    {
        // Go in reverse to start at the deepest dependency
        module_t* dependency = CONTAINER_OF_SAFE(list_pop_last(&ctx.dependencies), module_t, entry);
        if (dependency == NULL)
        {
            break;
        }

        if (module_call_load_event(dependency) == ERR)
        {
            module_free(dependency);
            goto error;
        }

        list_push_back(&loadedDependencies, &dependency->entry);
    }

    while (!list_is_empty(&loadedDependencies))
    {
        module_t* dependency = CONTAINER_OF(list_pop_first(&loadedDependencies), module_t, entry);
        LOG_DEBUG("finished loading dependency module '%s'\n", dependency->info->name);
    }

    if (module_call_load_event(module) == ERR)
    {
        goto error;
    }

    LOG_DEBUG("finished loading module '%s'\n", module->info->name);

    return module;

error:
    while (!list_is_empty(&loadedDependencies))
    {
        module_dependency_t* dependency = CONTAINER_OF(list_pop_last(&loadedDependencies), module_dependency_t, entry);
        module_call_unload_event(dependency->module);
        module_free(dependency->module);
    }
    while (!list_is_empty(&ctx.dependencies))
    {
        module_t* dependency = CONTAINER_OF(list_pop_first(&ctx.dependencies), module_t, entry);
        module_free(dependency);
    }
    module_free(module);
    return NULL;
}

static module_device_t* module_device_get_or_create(const char* deviceId)
{
    map_key_t deviceKey = map_key_string(deviceId);
    module_device_t* moduleDevice = CONTAINER_OF_SAFE(map_get(&deviceMap, &deviceKey), module_device_t, mapEntry);
    if (moduleDevice == NULL)
    {
        moduleDevice = malloc(sizeof(module_device_t));
        if (moduleDevice == NULL)
        {
            return NULL;
        }
        map_entry_init(&moduleDevice->mapEntry);
        list_init(&moduleDevice->handlers);
        strncpy_s(moduleDevice->id, MODULE_MAX_DEVICE_ID, deviceId, MODULE_MAX_DEVICE_ID);

        if (map_insert(&deviceMap, &deviceKey, &moduleDevice->mapEntry) == ERR)
        {
            free(moduleDevice);
            return NULL;
        }
    }

    return moduleDevice;
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
        module_cached_symbol_t* symbolEntry = CONTAINER_OF(entry, module_cached_symbol_t, mapEntry);
        free(symbolEntry->modulePath);
        free(symbolEntry);
    }
    map_clear(&symbolCache);

    for (uint64_t i = 0; i < deviceIdCache.capacity; i++)
    {
        map_entry_t* entry = deviceIdCache.entries[i];
        if (!MAP_ENTRY_PTR_IS_VALID(entry))
        {
            continue;
        }
        module_cached_device_t* deviceEntry = CONTAINER_OF(entry, module_cached_device_t, mapEntry);
        free(deviceEntry->paths);
        free(deviceEntry);
    }
    map_clear(&deviceIdCache);

    cacheValid = false;
}

static uint64_t module_cache_symbols_add(Elf64_File* elf, const char* path)
{
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

        Elf64_Symbol_Binding binding = ELF64_ST_BIND(sym->st_info);
        Elf64_Symbol_Type type = ELF64_ST_TYPE(sym->st_info);
        const char* symName = elf64_get_symbol_name(elf, sym);
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

static uint64_t module_cache_device_ids_add(module_info_t* info, const char* path)
{
    const char* deviceIdPtr = info->deviceIds;
    while (*deviceIdPtr != '\0')
    {
        char currentDeviceId[MODULE_MAX_DEVICE_ID] = {0};
        uint64_t parsed = module_parse_string(deviceIdPtr, currentDeviceId, MODULE_MAX_DEVICE_ID);
        if (parsed == 0)
        {
            break;
        }
        deviceIdPtr += parsed + 1;

        map_key_t deviceKey = map_key_string(currentDeviceId);
        module_cached_device_t* deviceEntry =
            CONTAINER_OF_SAFE(map_get(&deviceIdCache, &deviceKey), module_cached_device_t, mapEntry);
        if (deviceEntry == NULL)
        {
            deviceEntry = malloc(sizeof(module_cached_device_t));
            if (deviceEntry == NULL)
            {
                return ERR;
            }
            map_entry_init(&deviceEntry->mapEntry);
            deviceEntry->paths = strdup(path);
            if (deviceEntry->paths == NULL)
            {
                free(deviceEntry);
                return ERR;
            }
            deviceEntry->pathCount = 1;
            deviceEntry->pathsSize = strlen(path) + 1;
            if (map_insert(&deviceIdCache, &deviceKey, &deviceEntry->mapEntry) == ERR)
            {
                free(deviceEntry->paths);
                free(deviceEntry);
                return ERR;
            }
            continue;
        }

        size_t currentLen = deviceEntry->pathsSize - 1;
        size_t additionalLen = strlen(path);
        char* newPaths = realloc(deviceEntry->paths, deviceEntry->pathsSize + additionalLen + 1);
        if (newPaths == NULL)
        {
            return ERR;
        }
        deviceEntry->paths = newPaths;
        deviceEntry->pathCount++;
        deviceEntry->pathsSize += additionalLen + 1;

        memcpy(&newPaths[currentLen + 1], path, additionalLen + 1);
    }

    return 0;
}

static uint64_t module_cache_build(void)
{
    if (cacheValid)
    {
        return 0;
    }

    process_t* process = sched_process();
    assert(process != NULL);

    file_t* dir = vfs_open(PATHNAME(MODULE_DIR), process);
    if (dir == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(dir);

    dirent_t buffer[64];
    while (true)
    {
        uint64_t readCount = vfs_getdents(dir, buffer, sizeof(buffer));
        if (readCount == ERR)
        {
            module_cache_clear();
            return ERR;
        }
        if (readCount == 0)
        {
            break;
        }

        uint64_t direntCount = readCount / sizeof(dirent_t);
        for (uint64_t i = 0; i < direntCount; i++)
        {
            if (buffer[i].path[0] == '.' || buffer[i].type != INODE_FILE)
            {
                continue;
            }

            Elf64_File elf;
            module_info_t* info;
            if (module_read_metadata(&elf, &info, &dir->path, process, buffer[i].path) == ERR)
            {
                if (errno == EILSEQ)
                {
                    LOG_ERR("skipping invalid module file '%s'\n", buffer[i].path);
                    continue;
                }
                module_cache_clear();
                return ERR;
            }

            if (module_cache_symbols_add(&elf, buffer[i].path) == ERR)
            {
                free(elf.header);
                free(info);
                module_cache_clear();
                return ERR;
            }

            if (module_cache_device_ids_add(info, buffer[i].path) == ERR)
            {
                free(elf.header);
                free(info);
                module_cache_clear();
                return ERR;
            }

            LOG_DEBUG("built cache entry for module '%s'\n", info->name);

            free(elf.header);
            free(info);
        }
    }

    cacheValid = true;
    return 0;
}

uint64_t module_load(const char* deviceId, module_load_flags_t flags)
{
    if (deviceId == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&lock);

    if (module_cache_build() == ERR)
    {
        return ERR;
    }

    file_t* dir = vfs_open(PATHNAME(MODULE_DIR), sched_process());
    if (dir == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(dir);

    module_device_t* device = module_device_get_or_create(deviceId);
    if (device == NULL)
    {
        return ERR;
    }

    if (!(flags & MODULE_LOAD_ALL) && !list_is_empty(&device->handlers))
    {
        return 0;
    }

    list_t modulesLoaded = LIST_CREATE(modulesLoaded);
    map_key_t deviceKey = map_key_string(deviceId);
    module_cached_device_t* deviceEntry =
        CONTAINER_OF_SAFE(map_get(&deviceIdCache, &deviceKey), module_cached_device_t, mapEntry);
    if (deviceEntry == NULL)
    {
        return 0;
    }

    char* pathPtr = deviceEntry->paths;
    for (uint64_t i = 0; i < deviceEntry->pathCount; i++)
    {
        size_t pathLen = strnlen_s(pathPtr, MAX_PATH);
        if (pathLen == 0 || pathLen >= MAX_PATH)
        {
            errno = EILSEQ;
            goto error;
        }

        module_t* module = module_get_or_load_filename(dir, pathPtr, deviceId);
        if (module == NULL)
        {
            if (errno == ENODEV)
            {
                pathPtr += pathLen + 1;
                continue;
            }
            goto error;
        }

        list_push_back(&modulesLoaded, &module->entry);
        if (module_device_attach(device, module) == ERR)
        {
            goto error;
        }

        if (!(flags & MODULE_LOAD_ALL))
        {
            break;
        }

        pathPtr += pathLen + 1;
    }

    while (!list_is_empty(&modulesLoaded))
    {
        module_t* module = CONTAINER_OF(list_pop_first(&modulesLoaded), module_t, entry);
        LOG_DEBUG("successfully loaded module '%s' for device '%s'\n", module->info->name, deviceId);
    }

    return 0;

error:
    while (!list_is_empty(&modulesLoaded))
    {
        module_t* module = CONTAINER_OF(list_pop_first(&modulesLoaded), module_t, entry);
        module_device_detach(device, module);
    }
    module_collect_garbage();
    return ERR;
}

uint64_t module_unload(const char* deviceId)
{
    if (deviceId == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&lock);

    map_key_t deviceKey = map_key_string(deviceId);
    module_device_t* device = CONTAINER_OF_SAFE(map_get(&deviceMap, &deviceKey), module_device_t, mapEntry);
    if (device == NULL)
    {
        return 0;
    }

    while (device->handlers.length > 0)
    {
        uint64_t volatile handlerCount = device->handlers.length;

        module_device_handler_t* handler =
            CONTAINER_OF(list_first(&device->handlers), module_device_handler_t, deviceEntry);
        module_device_detach(device, handler->module);

        if (handlerCount == 1) // Avoid use after free
        {
            break;
        }
    }
    module_collect_garbage();
    return 0;
}
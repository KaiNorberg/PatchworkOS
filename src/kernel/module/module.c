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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/elf.h>
#include <sys/list.h>

// Keyed by symbol group ids
static map_t dependencyMap = MAP_CREATE;

// Keyed by name
static map_t moduleMap = MAP_CREATE;

static mutex_t lock = MUTEX_CREATE(lock);

static uint64_t module_add(module_t* module)
{
    map_key_t dependKey = map_key_uint64(module->symbolGroupId);
    if (map_insert(&dependencyMap, &dependKey, &module->dependencyMapEntry) == ERR)
    {
        return ERR;
    }

    map_key_t moduleKey = map_key_string(module->info.name);
    if (map_insert(&moduleMap, &moduleKey, &module->moduleMapEntry) == ERR)
    {
        map_remove(&dependencyMap, &dependKey);
        return ERR;
    }

    return 0;
}

static void module_remove(module_t* module)
{
    if (module == NULL)
    {
        return;
    }

    map_key_t dependKey = map_key_uint64(module->symbolGroupId);
    map_remove(&dependencyMap, &dependKey);
    map_key_t moduleKey = map_key_string(module->info.name);
    map_remove(&moduleMap, &moduleKey);
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

static void module_free(module_t* module)
{
    LOG_DEBUG("freeing resources for module '%s'\n", module->info.name);

    module_remove(module);
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

    free(module);
}

static module_t* module_new(module_info_t* info)
{
    module_t* module = malloc(sizeof(module_t));
    if (module == NULL)
    {
        LOG_ERR("failed to allocate memory for module structure (%s)", strerror(errno));
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
    map_init(&module->dependencies);
    memcpy_s(&module->info, sizeof(module_info_t), info, sizeof(module_info_t));

    if (module_add(module) == ERR)
    {
        free(module);
        return NULL;
    }

    return module;
}

void module_init_fake_kernel_module(const boot_kernel_t* kernel)
{
    module_info_t kernelInfo = {
        .name = "Kernel",
        .author = "Kai Norberg",
        .description = "The PatchworkOS kernel",
        .version = OS_VERSION,
        .licence = "MIT",
    };

    module_t* kernelModule = module_new(&kernelInfo);
    if (kernelModule == NULL)
    {
        panic(NULL, "Failed to create fake kernel module (%s)", strerror(errno));
    }
    kernelModule->flags |= MODULE_FLAG_LOADED;

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

static inline uint64_t module_load_parse_module_info(const char* moduleInfo, module_info_t* info)
{
    if (moduleInfo == NULL || info == NULL)
    {
        return ERR;
    }

    size_t offset = 0;
    size_t len = strnlen_s(&moduleInfo[offset], MODULE_MAX_NAME);
    if (len == MODULE_MAX_NAME)
    {
        return ERR;
    }
    strncpy_s(info->name, MODULE_MAX_NAME, &moduleInfo[offset], len + 1);
    offset += len + 1;

    len = strnlen_s(&moduleInfo[offset], MODULE_MAX_AUTHOR);
    if (len == MODULE_MAX_AUTHOR)
    {
        return ERR;
    }
    strncpy_s(info->author, MODULE_MAX_AUTHOR, &moduleInfo[offset], len + 1);
    offset += len + 1;

    len = strnlen_s(&moduleInfo[offset], MODULE_MAX_DESCRIPTION);
    if (len == MODULE_MAX_DESCRIPTION)
    {
        return ERR;
    }
    strncpy_s(info->description, MODULE_MAX_DESCRIPTION, &moduleInfo[offset], len + 1);
    offset += len + 1;

    len = strnlen_s(&moduleInfo[offset], MODULE_MAX_VERSION);
    if (len == MODULE_MAX_VERSION)
    {
        return ERR;
    }
    strncpy_s(info->version, MODULE_MAX_VERSION, &moduleInfo[offset], len + 1);
    offset += len + 1;

    len = strnlen_s(&moduleInfo[offset], MODULE_MAX_LICENCE);
    if (len == MODULE_MAX_LICENCE)
    {
        return ERR;
    }
    strncpy_s(info->licence, MODULE_MAX_LICENCE, &moduleInfo[offset], len + 1);
    offset += len + 1;

    if (moduleInfo[offset] != '\1')
    {
        return ERR;
    }

    return 0;
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

static uint64_t module_load_file(Elf64_File* outFile, module_info_t* outInfo, module_load_ctx_t* ctx,
    const char* filename)
{
    file_t* file = vfs_openat(&ctx->dir->path, PATHNAME(filename), ctx->process);
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
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    Elf64_Shdr* moduleInfoShdr = elf64_get_section_by_name(outFile, ".module_info");
    if (moduleInfoShdr == NULL || moduleInfoShdr->sh_size < MODULE_MIN_INFO ||
        moduleInfoShdr->sh_size > MODULE_MAX_INFO)
    {
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    if (module_load_parse_module_info(ELF64_AT_OFFSET(outFile, moduleInfoShdr->sh_offset), outInfo) == ERR)
    {
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    return 0; // Caller owns fileData in outFile->header
}

static uint64_t module_load_from_file(module_t* module, Elf64_File* elf, module_load_ctx_t* ctx)
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
        void* symAddr = (void*)((uintptr_t)module->baseAddr + (sym->st_value - minVaddr));
        Elf64_Symbol_Binding binding = ELF64_ST_BIND(sym->st_info);
        Elf64_Symbol_Type type = ELF64_ST_TYPE(sym->st_info);

        if (strncmp(symName, MODULE_RESERVED_PREFIX, MODULE_RESERVED_PREFIX_LENGTH) == 0)
        {
            continue;
        }

        if (symbol_add(symName, symAddr, module->symbolGroupId, binding, type) == ERR)
        {
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

static uint64_t module_find_and_load_dependency(module_load_ctx_t* ctx, const char* symbolName)
{
    dirent_t buffer[64];
    vfs_seek(ctx->dir, 0, SEEK_SET);

    while (true)
    {
        uint64_t readCount = vfs_getdents(ctx->dir, buffer, sizeof(buffer));
        if (readCount == ERR)
        {
            return ERR;
        }
        if (readCount == 0)
        {
            break;
        }

        uint64_t direntCount = readCount / sizeof(dirent_t);
        for (uint64_t i = 0; i < direntCount; i++)
        {
            if (buffer[i].path[0] == '.' || buffer[i].type != INODE_FILE || strcmp(buffer[i].path, ctx->filename) == 0)
            {
                continue;
            }

            Elf64_File elf;
            module_info_t info;
            if (module_load_file(&elf, &info, ctx, buffer[i].path) == ERR)
            {
                return ERR;
            }

            if (elf64_get_symbol_by_name(&elf, symbolName) == NULL)
            {
                free(elf.header);
                continue;
            }

            map_key_t moduleKey = map_key_string(info.name);
            module_t* existingModule = CONTAINER_OF_SAFE(map_get(&moduleMap, &moduleKey), module_t, moduleMapEntry);
            if (existingModule != NULL)
            {
                free(elf.header);
                return 0;
            }

            module_t* dependency = module_new(&info);
            if (dependency == NULL)
            {
                free(elf.header);
                return ERR;
            }
            dependency->flags |= MODULE_FLAG_DEPENDENCY;

            LOG_INFO(
                "loading dependency '%s'\n  Author:      %s\n  Description: %s\n  Version:     %s\n  Licence:     %s\n",
                dependency->info.name, dependency->info.author, dependency->info.description, dependency->info.version,
                dependency->info.licence);

            list_push_back(&ctx->dependencies, &dependency->entry);

            if (module_load_from_file(dependency, &elf, ctx) == ERR)
            {
                free(elf.header);
                list_remove(&ctx->dependencies, &dependency->entry);
                module_free(dependency);
                return ERR;
            }

            free(elf.header);
            return 0;
        }
    }

    LOG_ERR("failed to find module providing symbol '%s'\n", symbolName);
    return ERR;
}

static void* module_resolve_callback(const char* name, void* private)
{
    module_load_ctx_t* ctx = private;

    symbol_info_t symbolInfo;
    if (symbol_resolve_name(&symbolInfo, name) == ERR)
    {
        if (module_find_and_load_dependency(ctx, name) == ERR)
        {
            return NULL;
        }

        if (symbol_resolve_name(&symbolInfo, name) == ERR)
        {
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

uint64_t module_load(const char* directory, const char* filename)
{
    if (directory == NULL || filename == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&lock);

    module_load_ctx_t ctx = {
        .dir = vfs_open(PATHNAME(directory), sched_process()),
        .filename = filename,
        .process = sched_process(),
        .dependencies = LIST_CREATE(ctx.dependencies),
    };
    if (ctx.dir == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(ctx.dir);

    Elf64_File elf;
    module_info_t info;
    if (module_load_file(&elf, &info, &ctx, filename) == ERR)
    {
        return ERR;
    }

    map_key_t moduleKey = map_key_string(info.name);
    module_t* existingModule = CONTAINER_OF_SAFE(map_get(&moduleMap, &moduleKey), module_t, moduleMapEntry);
    if (existingModule != NULL)
    {
        if (existingModule->flags & MODULE_FLAG_DEPENDENCY)
        {
            existingModule->flags &= ~MODULE_FLAG_DEPENDENCY;
        }

        return 0;
    }

    module_t* module = module_new(&info);
    if (module == NULL)
    {
        free(elf.header);
        return ERR;
    }

    LOG_INFO("loading module '%s'\n  Author:      %s\n  Description: %s\n  Version:     %s\n  Licence:     %s\n",
        module->info.name, module->info.author, module->info.description, module->info.version, module->info.licence);

    uint64_t result = module_load_from_file(module, &elf, &ctx);
    free(elf.header);
    if (result == ERR)
    {
        module_free(module);
        goto dependency_error;
    }

    while (!list_is_empty(&ctx.dependencies))
    {
        // Go in reverse to start at the deepest dependency
        module_t* dependency = CONTAINER_OF_SAFE(list_pop_last(&ctx.dependencies), module_t, entry);
        if (dependency == NULL)
        {
            break;
        }

        module_event_t loadEvent = {
            .type = MODULE_EVENT_LOAD,
        };
        if (dependency->procedure(&loadEvent) == ERR)
        {
            module_free(dependency);
            goto dependency_error;
        }
        dependency->flags |= MODULE_FLAG_LOADED;

        LOG_DEBUG("finished loading dependency module '%s'\n", dependency->info.name);
    }

    module_event_t loadEvent = {
        .type = MODULE_EVENT_LOAD,
    };
    if (module->procedure(&loadEvent) == ERR)
    {
        for (uint64_t i = 0; i < module->dependencies.capacity; i++)
        {
            map_entry_t* entry = module->dependencies.entries[i];
            if (!MAP_ENTRY_PTR_IS_VALID(entry))
            {
                continue;
            }
            module_dependency_t* dependency = CONTAINER_OF(entry, module_dependency_t, entry);
            module_call_unload_event(dependency->module);
            module_free(dependency->module);
        }
        module_free(module);
        return ERR;
    }
    module->flags |= MODULE_FLAG_LOADED;

    LOG_DEBUG("finished loading module '%s'\n", module->info.name);

    return 0;

dependency_error:
    while (!list_is_empty(&ctx.dependencies))
    {
        module_t* dependency = CONTAINER_OF(list_pop_first(&ctx.dependencies), module_t, entry);
        module_free(dependency);
    }
    return ERR;
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
static void module_gc_collect_unreachable(module_t* module, list_t* toFree)
{
    if (module == NULL || module->flags & MODULE_FLAG_GC_REACHABLE)
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
        module_gc_collect_unreachable(dependency->module, toFree);
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
        if (!(module->flags & MODULE_FLAG_DEPENDENCY))
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
        module_gc_collect_unreachable(module, &toFree);
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

uint64_t module_unload(const char* name)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&lock);

    map_key_t moduleKey = map_key_string(name);
    module_t* module = CONTAINER_OF_SAFE(map_get(&moduleMap, &moduleKey), module_t, moduleMapEntry);
    if (module == NULL)
    {
        return ERR;
    }

    if (module->flags & MODULE_FLAG_DEPENDENCY)
    {
        LOG_WARN("module '%s' is a dependency and cannot be unloaded directly\n", module->info.name);
        errno = EBUSY;
        return ERR;
    }

    LOG_DEBUG("demoting module '%s' to a dependency\n", module->info.name);
    module->flags |= MODULE_FLAG_DEPENDENCY;

    module_collect_garbage();
    return 0;
}

#ifdef TESTING
void module_test()
{
    LOG_INFO("starting module tests...\n");
    for (uint64_t i = 0; i < 3; i++)
    {
        if (module_load("/kernel/modules:dir", "helloworld") == ERR)
        {
            panic(NULL, "Failed to load hello world module (%s)\n", strerror(errno));
        }

        module_unload("Hello World");
    }

    if (moduleMap.length != 1) // Kernel module remains
    {
        panic(NULL, "Module map not empty after unloading hello world module, %llu modules remaining\n",
            moduleMap.length);
    }

    for (uint64_t i = 0; i < 3; i++)
    {
        if (module_load("/kernel/modules:dir", "circular_depend1") == ERR)
        {
            panic(NULL, "Failed to load circular depend modules (%s)\n", strerror(errno));
        }

        module_unload("Circular Depend1");
    }

    if (moduleMap.length != 1)
    {
        panic(NULL, "Module map not empty after unloading circular depend modules, %llu modules remaining\n",
            moduleMap.length);
    }

    LOG_INFO("module tests completed\n");
}
#endif
#include <kernel/module/module.h>

#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/symbol.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/elf.h>

static list_t loadedmodules = LIST_CREATE(loadedmodules);
static mutex_t lock = MUTEX_CREATE(lock);

static void module_free(module_t* module)
{
    if (module == NULL)
    {
        return;
    }

    if (module->baseAddr != NULL)
    {
        vmm_unmap(NULL, module->baseAddr, module->size);
    }

    free(module);
}

static module_t* module_new(void)
{
    module_t* module = malloc(sizeof(module_t));
    if (module == NULL)
    {
        LOG_ERR("failed to allocate memory for module structure (%s)", strerror(errno));
        return NULL;
    }
    ref_init(&module->ref, module_free);
    list_entry_init(&module->entry);
    module->baseAddr = NULL;
    module->size = 0;
    module->procedure = NULL;
    module->symbolGroupId = symbol_generate_group_id();
    module->name[0] = '\0';
    module->author[0] = '\0';
    module->description[0] = '\0';
    module->version[0] = '\0';
    module->licence[0] = '\0';
    return module;
}

static inline uint64_t module_load_parse_module_info(const char* moduleInfo, module_t* module)
{
    if (moduleInfo == NULL || module == NULL)
    {
        return ERR;
    }

    size_t offset = 0;
    size_t len = strnlen_s(&moduleInfo[offset], MODULE_MAX_NAME);
    if (len == MODULE_MAX_NAME)
    {
        return ERR;
    }
    strncpy(module->name, &moduleInfo[offset], len + 1);
    offset += len + 1;

    len = strnlen_s(&moduleInfo[offset], MODULE_MAX_AUTHOR);
    if (len == MODULE_MAX_AUTHOR)
    {
        return ERR;
    }
    strncpy(module->author, &moduleInfo[offset], len + 1);
    offset += len + 1;

    len = strnlen_s(&moduleInfo[offset], MODULE_MAX_DESCRIPTION);
    if (len == MODULE_MAX_DESCRIPTION)
    {
        return ERR;
    }
    strncpy(module->description, &moduleInfo[offset], len + 1);
    offset += len + 1;

    len = strnlen_s(&moduleInfo[offset], MODULE_MAX_VERSION);
    if (len == MODULE_MAX_VERSION)
    {
        return ERR;
    }
    strncpy(module->version, &moduleInfo[offset], len + 1);
    offset += len + 1;

    len = strnlen_s(&moduleInfo[offset], MODULE_MAX_LICENCE);
    if (len == MODULE_MAX_LICENCE)
    {
        return ERR;
    }
    strncpy(module->licence, &moduleInfo[offset], len + 1);
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
    process_t* process;
    list_t loadedModules;
    list_t initializedModules;
} module_load_ctx_t;

static Elf64_Addr module_resolve_callback(const char* name, void* private);

static uint64_t module_load_file(Elf64_File* outFile, module_load_ctx_t* ctx, const char* filename)
{
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/%s", ctx->directory, filename);

    file_t* file = vfs_open(PATHNAME(path), ctx->process);
    if (file == NULL)
    {
        LOG_ERR("failed to open module file '%s' (%s)\n", path, strerror(errno));
        return ERR;
    }
    DEREF_DEFER(file);

    uint64_t fileSize = vfs_seek(file, 0, SEEK_END);
    vfs_seek(file, 0, SEEK_SET);
    if (fileSize == ERR)
    {
        LOG_ERR("failed to seek module file '%s' (%s)\n", path, strerror(errno));
        return ERR;
    }

    uint8_t* fileData = malloc(fileSize);
    if (fileData == NULL)
    {
        LOG_ERR("failed to allocate memory for module file '%s' (%s)\n", path, strerror(errno));
        return ERR;
    }

    if (vfs_read(file, fileData, fileSize) == ERR)
    {
        LOG_ERR("failed to read module file '%s' (%s)\n", path, strerror(errno));
        free(fileData);
        return ERR;
    }

    if (elf64_validate(outFile, fileData, fileSize) == ERR)
    {
        LOG_ERR("module file '%s' is not a valid ELF file\n", path);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    return 0; // Called owns fileData in outFile->header
}

static uint64_t module_load_dependency(module_load_ctx_t* ctx, const char* filename)
{
    Elf64_File elf;
    if (module_load_file(&elf, ctx, filename) == ERR)
    {
        return ERR;
    }

    module_t* module = module_new();
    if (module == NULL)
    {
        LOG_ERR("failed to allocate memory for module structure (%s)\n", strerror(errno));
        free(elf.header);
        return ERR;
    }
    DEREF_DEFER(module);

    Elf64_Shdr* moduleInfoShdr = elf64_get_section_by_name(&elf, ".module_info");
    if (moduleInfoShdr == NULL)
    {
        LOG_ERR("module file '%s' is missing .module_info section\n", filename);
        free(elf.header);
        errno = EILSEQ;
        return ERR;
    }

    if (moduleInfoShdr->sh_size < 6 || moduleInfoShdr->sh_size > MODULE_MAX_INFO)
    {
        LOG_ERR("module file '%s' has invalid .module_info section size\n", filename);
        free(elf.header);
        errno = EILSEQ;
        return ERR;
    }

    if (module_load_parse_module_info(ELF64_AT_OFFSET(&elf, moduleInfoShdr->sh_offset), module) == ERR)
    {
        LOG_ERR("module file '%s' has invalid .module_info section content\n", filename);
        free(elf.header);
        errno = EILSEQ;
        return ERR;
    }

    LOG_INFO("loading module '%s'\n  Author:      %s\n  Description: %s\n  Version:     %s\n  Licence:     %s\n",
        module->name, module->author, module->description, module->version, module->licence);

    Elf64_Addr minVaddr;
    Elf64_Addr maxVaddr;
    elf64_get_loadable_bounds(&elf, &minVaddr, &maxVaddr);
    uint64_t moduleMemSize = maxVaddr - minVaddr;

    // Will be unmaped in module_free
    module->baseAddr = vmm_alloc(NULL, NULL, moduleMemSize, PML_PRESENT | PML_WRITE | PML_GLOBAL, VMM_ALLOC_NONE);
    if (module->baseAddr == NULL)
    {
        LOG_ERR("failed to allocate memory for module '%s' (%s)\n", module->name, strerror(errno));
        free(elf.header);
        return ERR;
    }
    module->size = moduleMemSize;
    module->procedure = (module_procedure_t)((uintptr_t)module->baseAddr + (elf.header->e_entry - minVaddr));

    elf64_load_segments(&elf, (Elf64_Addr)module->baseAddr, minVaddr);

    if (elf64_relocate(&elf, (Elf64_Addr)module->baseAddr, minVaddr, module_resolve_callback, ctx) == ERR)
    {
        LOG_ERR("failed to relocate module '%s'\n", module->name);
        free(elf.header);
        return ERR;
    }

    uint64_t index = 0;
    while (true)
    {
        Elf64_Sym* sym = elf64_get_symbol_by_index(&elf, index++);
        if (sym == NULL)
        {
            break;
        }

        if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_ABS)
        {
            continue;
        }

        const char* symName = elf64_get_symbol_name(&elf, sym);
        void* symAddr = (void*)((uintptr_t)module->baseAddr + (sym->st_value - minVaddr));
        Elf64_Symbol_Binding binding = ELF64_ST_BIND(sym->st_info);
        Elf64_Symbol_Type type = ELF64_ST_TYPE(sym->st_info);
        if (symbol_add(symName, symAddr, module->symbolGroupId, binding, type) == ERR)
        {
            LOG_ERR("failed to add symbol '%s' from module '%s' (%s)\n", symName, module->name, strerror(errno));
            free(elf.header);
            return ERR;
        }
    }

    free(elf.header);
    list_push(&ctx->loadedModules, &REF(module)->entry);
    return 0;
}

static uint64_t module_find_and_load_dependency(module_load_ctx_t* ctx, const char* symbolName)
{
    dirent_t buffer[64];

    file_t*

    while (true)
    {
        uint64_t readCount = vfs_getdents()
    }
}

static Elf64_Addr module_resolve_callback(const char* name, void* private)
{
    module_load_ctx_t* ctx = private;

    symbol_info_t symbolInfo;
    if (symbol_resolve_name(&symbolInfo, name) == ERR)
    {
        LOG_ERR("failed to resolve symbol '%s' for module (%s)\n", name, strerror(errno));
        return 0;
    }
    return (Elf64_Addr)symbolInfo.addr;
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
        .directory = directory,
        .process = sched_process(),
        .loadedModules = LIST_CREATE(ctx.loadedModules),
    };

    if (module_load_dependency(&ctx, filename) == ERR)
    {
        return ERR;
    }

    // Go in reverse to start at the deepest dependency
    module_t* module;
    LIST_FOR_EACH_REVERSE(module, &ctx.loadedModules, entry)
    {
        module_event_t event = {
            .type = MODULE_EVENT_LOAD,
        };
        if (module->procedure(&event) == ERR)
        {
            LOG_ERR("module '%s' failed to initialize\n", module->name);
            goto error;
        }
    }

    return 0;

error:
    while (!list_is_empty(&ctx.initializedModules))
    {
        module = CONTAINER_OF(list_pop(&ctx.initializedModules), module_t, entry);
        module_event_t event = {
            .type = MODULE_EVENT_UNLOAD,
        };
        module->procedure(&event);
        DEREF(module);
    }
    while (!list_is_empty(&ctx.loadedModules))
    {
        module = CONTAINER_OF(list_pop(&ctx.loadedModules), module_t, entry);
        DEREF(module);
    }
    return ERR;
}

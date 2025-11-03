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

static list_t loadedModules = LIST_CREATE(loadedModules);
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
        LOG_ERR("Failed to allocate memory for module structure (%s)", strerror(errno));
        return NULL;
    }
    ref_init(&module->ref, module_free);
    list_entry_init(&module->entry);
    module->baseAddr = NULL;
    module->size = 0;
    module->procedure = NULL;
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

static Elf64_Addr module_load_resolve_callback(const char* name)
{
    symbol_info_t symbolInfo;
    if (symbol_resolve_name(&symbolInfo, name) == ERR)
    {
        LOG_ERR("Failed to resolve symbol '%s' for module (%s)", name, strerror(errno));
        return ERR;
    }
    return (Elf64_Addr)symbolInfo.addr;
}

uint64_t module_load(const char* path)
{
    if (path == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&lock);

    process_t* process = sched_process();
    file_t* file = vfs_open(PATHNAME(path), process);
    if (file == NULL)
    {
        LOG_ERR("Failed to open module file '%s' (%s)", path, strerror(errno));
        return ERR;
    }
    DEREF_DEFER(file);

    uint64_t fileSize = vfs_seek(file, 0, SEEK_END);
    vfs_seek(file, 0, SEEK_SET);
    if (fileSize == ERR)
    {
        LOG_ERR("Failed to seek module file '%s' (%s)", path, strerror(errno));
        return ERR;
    }

    uint8_t* fileData = malloc(fileSize);
    if (fileData == NULL)
    {
        LOG_ERR("Failed to allocate memory for module file '%s' (%s)", path, strerror(errno));
        return ERR;
    }

    if (vfs_read(file, fileData, fileSize) == ERR)
    {
        LOG_ERR("Failed to read module file '%s' (%s)", path, strerror(errno));
        free(fileData);
        return ERR;
    }

    Elf64_File elf;
    if (elf64_validate(&elf, fileData, fileSize) == ERR)
    {
        LOG_ERR("Module file '%s' is not a valid ELF file", path);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    Elf64_Shdr* moduleInfoShdr = elf64_get_section_by_name(&elf, ".module_info");
    if (moduleInfoShdr == NULL)
    {
        LOG_ERR("Module file '%s' is missing .module_info section", path);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    if (moduleInfoShdr->sh_size < 6 || moduleInfoShdr->sh_size > MODULE_MAX_INFO)
    {
        LOG_ERR("Module file '%s' has invalid .module_info section size", path);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    module_t* module = module_new();
    if (module == NULL)
    {
        LOG_ERR("Failed to allocate memory for module structure (%s)", strerror(errno));
        free(fileData);
        return ERR;
    }
    DEREF_DEFER(module);

    if (module_load_parse_module_info(ELF64_AT_OFFSET(&elf, moduleInfoShdr->sh_offset), module) == ERR)
    {
        LOG_ERR("Module file '%s' has invalid .module_info section content", path);
        free(fileData);
        errno = EILSEQ;
        return ERR;
    }

    LOG_INFO("Loading module '%s'\n  Author:      %s\n  Description: %s\n  Version:     %s\n  Licence:     %s\n",
        module->name, module->author, module->description, module->version, module->licence);

    Elf64_Addr minVaddr;
    Elf64_Addr maxVaddr;
    elf64_get_loadable_bounds(&elf, &minVaddr, &maxVaddr);
    uint64_t moduleMemSize = maxVaddr - minVaddr;

    module->baseAddr = vmm_alloc(NULL, NULL, moduleMemSize, PML_PRESENT | PML_WRITE | PML_GLOBAL, VMM_ALLOC_NONE);
    if (module->baseAddr == NULL)
    {
        LOG_ERR("Failed to allocate memory for module '%s' (%s)", module->name, strerror(errno));
        free(fileData);
        return ERR;
    }
    module->size = moduleMemSize;
    module->procedure = (module_procedure_t)((uintptr_t)module->baseAddr + (elf.header->e_entry - minVaddr));

    elf64_load_segments(&elf, (Elf64_Addr)module->baseAddr, minVaddr);

    if (elf64_relocate(&elf, (Elf64_Addr)module->baseAddr, minVaddr, module_load_resolve_callback) == ERR)
    {
        LOG_ERR("Failed to relocate module '%s'", module->name);
        free(fileData);
        return ERR;
    }

    module_event_t event = {
        .type = MODULE_EVENT_LOAD,
    };
    if (module->procedure(&event) == ERR)
    {
        LOG_ERR("Module '%s' failed to initialize", module->name);
        free(fileData);
        return ERR;
    }

    list_push(&loadedModules, &REF(module)->entry);
    return 0;
}

#include <kernel/module/module.h>

#include <kernel/fs/sysfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/sglist.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/symbol.h>
#include <kernel/start/boot_info.h>
#include <kernel/sync/lock.h>

#include <libc/elf.h>
#include <stdlib.h>
#include <string.h>

static dentry_t* instancesDir = NULL;
static dentry_t* loadFile = NULL;
static dentry_t* modDir = NULL;

static list_t modulesList = LIST_CREATE(modulesList);
static mutex_t lock = MUTEX_CREATE(lock);

#define MODULE_SYMBOL_ALLOWED(type, binding, name) \
    (((type) == STT_OBJECT || (type) == STT_FUNC) && ((binding) == STB_GLOBAL) && \
        (strncmp(name, MODULE_RESERVED_PREFIX, MODULE_RESERVED_PREFIX_LENGTH) != 0))

static void* module_resolve_symbol_callback(const char* name, void* data)
{
    UNUSED(data);
    symbol_info_t symbolInfo;
    if (IS_ERR(symbol_resolve_name(&symbolInfo, name)))
    {
        LOG_ERR("failed to resolve symbol '%s'\n", name);
        return NULL;
    }
    return symbolInfo.addr;
}

static vnode_class_t dirClass = {
    .name = "mod dir",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

static status_t module_attach_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    sglist_t* buffer = frame->write.buffer;
    size_t size = sglist_size(buffer);

    if (size == 0)
    {
        return ERR(MODULE, INVAL);
    }

    void* data = malloc(size);
    if (data == NULL)
    {
        return ERR(MODULE, NOMEM);
    }

    size_t copied;
    status_t status = sglist_copy_out(buffer, size, 0, &copied, data, size);
    if (IS_ERR(status))
    {
        free(data);
        return status;
    }

    size_t typeLen = 0;
    while (typeLen < size && ((char*)data)[typeLen] != '\0')
    {
        typeLen++;
    }
    if (typeLen == size)
    {
        free(data);
        return ERR(MODULE, INVAL);
    }
    const char* deviceType = (const char*)data;
    size_t offset = typeLen + 1;

    size_t nameLen = 0;
    while (offset + nameLen < size && ((char*)data)[offset + nameLen] != '\0')
    {
        nameLen++;
    }
    if (offset + nameLen == size)
    {
        free(data);
        return ERR(MODULE, INVAL);
    }
    const char* deviceName = (const char*)data + offset;

    module_t* module = frame->vnode->data;

    module_event_t attachEvent = {
        .type = MODULE_EVENT_DEVICE_ATTACH,
        .deviceAttach.type = deviceType,
        .deviceAttach.name = deviceName,
    };

    status = module->procedure(&attachEvent);
    free(data);

    if (IS_ERR(status))
    {
        return status;
    }

    irp->result = size;
    return status;
}

static vnode_class_t attachClass = {
    .name = "mod attach",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_WRITE] = module_attach_write,
        },
};

static status_t module_load_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    sglist_t* buffer = frame->write.buffer;
    size_t size = sglist_size(buffer);

    if (size == 0)
    {
        return ERR(MODULE, INVAL);
    }

    void* data = malloc(size);
    if (data == NULL)
    {
        return ERR(MODULE, NOMEM);
    }

    size_t copied;
    status_t status = sglist_copy_out(buffer, size, 0, &copied, data, size);
    if (IS_ERR(status))
    {
        free(data);
        return status;
    }

    size_t modNameLen = 0;
    while (modNameLen < size && ((char*)data)[modNameLen] != '\0')
    {
        modNameLen++;
    }
    if (modNameLen == size)
    {
        free(data);
        return ERR(MODULE, INVAL);
    }
    const char* moduleName = (const char*)data;
    size_t offset = modNameLen + 1;

    size_t typeLen = 0;
    while (offset + typeLen < size && ((char*)data)[offset + typeLen] != '\0')
    {
        typeLen++;
    }
    if (offset + typeLen == size)
    {
        free(data);
        return ERR(MODULE, INVAL);
    }
    const char* deviceType = (const char*)data + offset;
    offset += typeLen + 1;

    size_t nameLen = 0;
    while (offset + nameLen < size && ((char*)data)[offset + nameLen] != '\0')
    {
        nameLen++;
    }
    if (offset + nameLen == size)
    {
        free(data);
        return ERR(MODULE, INVAL);
    }
    const char* deviceName = (const char*)data + offset;
    offset += nameLen + 1;

    LOG_INFO("loading module '%s' for device '%s' of type '%s'\n", moduleName, deviceName, deviceType);

    Elf64_File elf;
    uint64_t res = elf64_validate(&elf, (uint8_t*)data + offset, size - offset);
    if (res != 0)
    {
        LOG_ERR("failed to validate ELF file while loading module (%llu)\n", res);
        free(data);
        return ERR(MODULE, INVALELF);
    }

    MUTEX_SCOPE(&lock);

    module_t* module = malloc(sizeof(module_t));
    if (module == NULL)
    {
        free(data);
        return ERR(MODULE, NOMEM);
    }

    list_entry_init(&module->listEntry);
    strncpy(module->name, moduleName, MAX_NAME - 1);
    module->name[MAX_NAME - 1] = '\0';
    module->baseAddr = NULL;
    module->size = 0;
    module->symbolGroupId = symbol_generate_group_id();

    Elf64_Addr minVaddr;
    Elf64_Addr maxVaddr;
    elf64_get_loadable_bounds(&elf, &minVaddr, &maxVaddr);
    uint64_t moduleMemSize = maxVaddr - minVaddr;

    status = vmm_alloc(NULL, &module->baseAddr, moduleMemSize, PAGE_SIZE, PML_PRESENT | PML_WRITE | PML_GLOBAL,
        VMM_ALLOC_OVERWRITE);
    if (IS_ERR(status))
    {
        free(module);
        free(data);
        return status;
    }

    module->size = moduleMemSize;
    elf64_load_segments(&elf, (Elf64_Addr)module->baseAddr, minVaddr);

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
        Elf64_Symbol_Binding binding = ELF64_ST_BIND(sym->st_info);
        Elf64_Symbol_Type type = ELF64_ST_TYPE(sym->st_info);
        if (!MODULE_SYMBOL_ALLOWED(type, binding, symName))
        {
            continue;
        }

        void* symAddr = (void*)((uintptr_t)module->baseAddr + (sym->st_value - minVaddr));
        status = symbol_add(symName, symAddr, module->symbolGroupId, binding, type);
        if (IS_ERR(status))
        {
            LOG_ERR("failed to add symbol '%s'\n", symName);
            vmm_unmap(NULL, module->baseAddr, module->size);
            symbol_remove_group(module->symbolGroupId);
            free(module);
            free(data);
            return status;
        }
    }

    if (!elf64_relocate(&elf, (Elf64_Addr)module->baseAddr, minVaddr, module_resolve_symbol_callback, NULL))
    {
        LOG_ERR("failed to relocate module\n");
        vmm_unmap(NULL, module->baseAddr, module->size);
        symbol_remove_group(module->symbolGroupId);
        free(module);
        free(data);
        return ERR(MODULE, INVALELF);
    }

    list_push_back(&modulesList, &module->listEntry);

    module->procedure = (module_procedure_t)((uintptr_t)module->baseAddr + (elf.header->e_entry - minVaddr));

    module_event_t loadEvent = {
        .type = MODULE_EVENT_LOAD,
    };
    status = module->procedure(&loadEvent);
    if (IS_ERR(status))
    {
        LOG_ERR("module entry point returned error %Y on load\n", status);
        list_remove(&module->listEntry);
        vmm_unmap(NULL, module->baseAddr, module->size);
        symbol_remove_group(module->symbolGroupId);
        free(module);
        free(data);
        return status;
    }

    module->instanceDir = sysfs_dentry_new(instancesDir, module->name, &dirClass, NULL);
    if (module->instanceDir == NULL)
    {
        LOG_ERR("failed to create module instance directory\n");
        module_event_t unloadEvent = {.type = MODULE_EVENT_UNLOAD};
        module->procedure(&unloadEvent);

        list_remove(&module->listEntry);
        vmm_unmap(NULL, module->baseAddr, module->size);
        symbol_remove_group(module->symbolGroupId);
        free(module);
        free(data);
        return ERR(MODULE, NOMEM);
    }

    module->attachFile = sysfs_dentry_new(module->instanceDir, "attach", &attachClass, module);
    if (module->attachFile == NULL)
    {
        UNREF(module->instanceDir);

        LOG_ERR("failed to create module attach file\n");
        module_event_t unloadEvent = {.type = MODULE_EVENT_UNLOAD};
        module->procedure(&unloadEvent);

        list_remove(&module->listEntry);
        vmm_unmap(NULL, module->baseAddr, module->size);
        symbol_remove_group(module->symbolGroupId);
        free(module);
        free(data);
        return ERR(MODULE, NOMEM);
    }

    if (strcmp(deviceType, "-") == 0 || strcmp(deviceName, "-") == 0)
    {
        LOG_INFO("module loaded successfully without attaching at %p\n", module->baseAddr);

        free(data);
        irp->result = size;
        return OK;
    }

    module_event_t attachEvent = {
        .type = MODULE_EVENT_DEVICE_ATTACH,
        .deviceAttach.type = deviceType,
        .deviceAttach.name = deviceName,
    };
    status = module->procedure(&attachEvent);
    if (IS_ERR(status))
    {
        LOG_ERR("module entry point returned error %Y on device attach\n", status);
        module_event_t unloadEvent = {.type = MODULE_EVENT_UNLOAD};
        module->procedure(&unloadEvent);

        UNREF(module->attachFile);
        UNREF(module->instanceDir);

        list_remove(&module->listEntry);
        vmm_unmap(NULL, module->baseAddr, module->size);
        symbol_remove_group(module->symbolGroupId);
        free(module);
        free(data);
        return status;
    }

    if (IS_CODE(status, DEFERRED))
    {
        LOG_INFO("module deferred at %p\n", module->baseAddr);
    }
    else
    {
        LOG_INFO("module loaded successfully at %p\n", module->baseAddr);
    }

    free(data);
    irp->result = size;
    return status;
}

static vnode_class_t loadClass = {
    .name = "mod load",
    .type = FILE_TYPE_DEVICE,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_WRITE] = module_load_write,
        },
};

void module_expose(void)
{
    modDir = sysfs_dentry_new(NULL, "mod", &dirClass, NULL);
    if (modDir == NULL)
    {
        panic(NULL, "failed to create /sys/mod directory\n");
    }

    loadFile = sysfs_dentry_new(modDir, "load", &loadClass, NULL);
    if (loadFile == NULL)
    {
        panic(NULL, "failed to create /sys/mod/load file\n");
    }

    instancesDir = sysfs_dentry_new(modDir, "instances", &dirClass, NULL);
    if (instancesDir == NULL)
    {
        panic(NULL, "failed to create /sys/mod/instances directory\n");
    }
}

status_t module_init_fake_kernel_module(void)
{
    boot_info_t* bootInfo = boot_info_get();
    const boot_kernel_t* kernel = &bootInfo->kernel;
    const Elf64_File* elf = &kernel->elf;

    module_t* kernelModule = malloc(sizeof(module_t));
    if (kernelModule == NULL)
    {
        return ERR(MODULE, NOMEM);
    }
    list_entry_init(&kernelModule->listEntry);
    strncpy(kernelModule->name, "kernel", MAX_NAME - 1);
    kernelModule->name[MAX_NAME - 1] = '\0';
    kernelModule->baseAddr = NULL;
    kernelModule->size = 0;
    kernelModule->symbolGroupId = 0;

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
            free(kernelModule);
            return status;
        }
    }

    list_push_back(&modulesList, &kernelModule->listEntry);

    LOG_INFO("loaded %llu kernel symbols\n", index);
    return OK;
}

#include <kernel/module/module.h>

#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sync/lock.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/elf.h>

static map_t hidMap = MAP_CREATE;
static mutex_t mutex;

static uint8_t* module_elf_load_section(file_t* file, const void* header, char* shstrtab, const char* sectionName)
{
    if (file == NULL || header == NULL || shstrtab == NULL || sectionName == NULL)
    {
        return NULL;
    }

    /*for (uint32_t i = 0; i < header->shdrAmount; i++)
    {
        uint64_t shdrOffset = header->shdrOffset + (i * header->shdrSize);
        if (vfs_seek(file, shdrOffset, SEEK_SET) == ERR)
        {
            return NULL;
        }

        elf_shdr_t shdr;
        if (vfs_read(file, &shdr, sizeof(elf_shdr_t)) != sizeof(elf_shdr_t))
        {
            return NULL;
        }

        const char* name = &shstrtab[shdr.name];
        if (strcmp(name, sectionName) != 0)
        {
            continue;
        }

        if (vfs_seek(file, shdr.offset, SEEK_SET) == ERR)
        {
            return NULL;
        }

        uint8_t* sectionData = malloc(shdr.size);
        if (sectionData == NULL)
        {
            return NULL;
        }
        if (vfs_read(file, sectionData, shdr.size) != shdr.size)
        {
            free(sectionData);
            return NULL;
        }
        return sectionData;
    }*/

    return NULL;
}

static char* module_elf_load_shstrtab(file_t* file, const void* header)
{
    if (file == NULL || header == NULL)
    {
        return NULL;
    }

    /*for (uint32_t i = 0; i < header->shdrAmount; i++)
    {
        elf_shdr_t shdr;
        uint64_t shdrOffset = header->shdrOffset + (i * header->shdrSize);
        if (vfs_seek(file, shdrOffset, SEEK_SET) == ERR)
        {
            return NULL;
        }

        if (vfs_read(file, &shdr, sizeof(elf_shdr_t)) != sizeof(elf_shdr_t))
        {
            return NULL;
        }

        if (shdr.type == ELF_SHDR_TYPE_STRTAB && i == header->shdrStringIndex)
        {
            char* shstrtab = malloc(shdr.size);
            if (shstrtab == NULL)
            {
                return NULL;
            }

            if (vfs_seek(file, shdr.offset, SEEK_SET) == ERR)
            {
                free(shstrtab);
                return NULL;
            }

            if (vfs_read(file, shstrtab, shdr.size) != shdr.size)
            {
                free(shstrtab);
                return NULL;
            }

            return shstrtab;
        }
    }*/

    return NULL;
}

static uint64_t module_register(const char* path)
{
    (void)path;
    /*char fullPath[MAX_PATH];
    int len = snprintf(fullPath, sizeof(fullPath), "/kernel/modules/%s", path);
    if (len < 0 || (uint64_t)len >= sizeof(fullPath))
    {
        LOG_ERR("module path too long '%s'\n", path);
        return ERR;
    }

    file_t* file = vfs_open(PATHNAME(fullPath), sched_process());
    if (file == NULL)
    {
        LOG_ERR("failed to open module file '%s'\n", path);
        return ERR;
    }
    DEREF_DEFER(file);

    elf_hdr_t header;
    if (vfs_read(file, &header, sizeof(elf_hdr_t)) != sizeof(elf_hdr_t))
    {
        LOG_ERR("failed to read ELF header from module '%s'\n", path);
        return ERR;
    }

    if (!ELF_IS_VALID(&header))
    {
        LOG_ERR("invalid ELF header in module '%s'\n", path);
        return ERR;
    }

    char* shstrtab = module_elf_load_shstrtab(file, &header);
    if (shstrtab == NULL)
    {
        LOG_ERR("failed to load string table for module '%s'\n", path);
        return ERR;
    }

    uint8_t* moduleInfo = module_elf_load_section(file, &header, shstrtab, ".module_info");
    if (moduleInfo == NULL)
    {
        LOG_ERR("failed to load .module_info section in module '%s'\n", path);
        free(shstrtab);
        return ERR;
    }

    uint8_t* moduleAcpiHids = module_elf_load_section(file, &header, shstrtab, ".module_acpi_hids");
    if (moduleAcpiHids == NULL)
    {
        LOG_ERR("failed to load .module_acpi_hids section in module '%s'\n", path);
        free(moduleInfo);
        free(shstrtab);
        return ERR;
    }

    free(shstrtab);

    module_t* module = malloc(sizeof(module_t));
    if (module == NULL)
    {
        LOG_ERR("failed to allocate memory for module '%s'\n", path);
        return ERR;
    }
    module->baseAddr = NULL;
    module->size = 0;
    module->procedure = NULL;
    list_init(&module->hidHandlers);
    module->file = NULL;
    module->flags = MODULE_FLAG_NONE;

    MUTEX_SCOPE(&mutex);

    uint64_t registeredCount = 0;

    char* hidPtr = (char*)moduleAcpiHids;
    while (*hidPtr != '\0' && *(hidPtr + 1) != '\1')
    {
        size_t hidLen = strnlen_s(hidPtr, MAX_NAME);
        if (hidLen == 0 || hidLen >= MAX_NAME)
        {
            LOG_ERR("invalid ACPI HID in module '%s'\n", path);
            goto error;
        }

        map_key_t mapKey = map_key_string(hidPtr);
        if (map_get(&hidMap, &mapKey) != NULL)
        {
            continue;
        }

        hid_handler_t* handler = malloc(sizeof(hid_handler_t));
        if (handler == NULL)
        {
            LOG_ERR("failed to allocate memory for ACPI HID map entry in module '%s'\n", path);
            goto error;
        }
        list_entry_init(&handler->moduleEntry);
        map_entry_init(&handler->hidMapEntry);
        handler->module = module;
        strncpy(handler->hid, hidPtr, MAX_NAME - 1);
        handler->hid[MAX_NAME - 1] = '\0';

        LOG_DEBUG("registering '%s' for module '%s'\n", hidPtr, path);
        if (map_insert(&hidMap, &mapKey, &handler->hidMapEntry) == ERR)
        {
            LOG_ERR("failed to register ACPI HID '%s' for module '%s'\n", hidPtr, path);
            free(handler);
            goto error;
        }

        list_push(&module->hidHandlers, &handler->moduleEntry);
        hidPtr += hidLen + 1;
        registeredCount++;
    }

    if (registeredCount == 0)
    {
        LOG_WARN("module '%s' does not register any ACPI HIDs\n", path);
        goto error;
    }

    module->file = REF(file);
    free(moduleAcpiHids);
    free(moduleInfo);
    return 0;

error:
    while (!list_is_empty(&module->hidHandlers))
    {
        hid_handler_t* handler = CONTAINER_OF_SAFE(list_pop(&module->hidHandlers), hid_handler_t, moduleEntry);
        map_key_t mapKey = map_key_string(handler->hid);
        map_remove(&hidMap, &mapKey);
        free(handler);
    }
    free(moduleAcpiHids);
    free(moduleInfo);
    free(module);*/
    return ERR;
}

void module_init(void)
{
    /*mutex_init(&mutex);

    file_t* moduleDir = vfs_open(PATHNAME("/kernel/modules:dir:recur"), sched_process());
    if (moduleDir == NULL)
    {
        LOG_WARN("no /kernel/modules directory found, skipping module registration\n");
        return;
    }
    DEREF_DEFER(moduleDir);

    uint64_t moduleCount = 0;

    const uint64_t bufferLen = 16;
    dirent_t buffer[bufferLen];
    while (true)
    {
        uint64_t readCount = vfs_getdents(moduleDir, buffer, sizeof(dirent_t) * bufferLen);
        if (readCount == ERR)
        {
            break;
        }

        if (readCount == 0)
        {
            break;
        }

        for (uint64_t i = 0; i < readCount / sizeof(dirent_t); i++)
        {
            if (buffer[i].type != INODE_FILE)
            {
                continue;
            }

            if (module_register(buffer[i].path) == ERR)
            {
                LOG_WARN("skipping module '%s'\n", buffer[i].path);
                continue;
            }
            moduleCount++;
        }
    }

    LOG_INFO("registered %llu modules\n", moduleCount);*/
}

static uint64_t module_load(module_t* module)
{
    if (module == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOG_INFO("loaded module at %p (size=%llu bytes)\n", module->baseAddr, module->size);
    return 0;
}

static hid_handler_t* module_hid_handler_lookup(const char* hid)
{
    if (hid == NULL)
    {
        return NULL;
    }
    return NULL;

    /*map_key_t mapKey = map_key_string(hid);
    map_entry_t* entry = map_get(&hidMap, &mapKey);
    if (entry == NULL)
    {
        return NULL;
    }

    return CONTAINER_OF(entry, hid_handler_t, hidMapEntry);*/
}

uint64_t module_event(module_event_t* event)
{
    if (event == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    /*switch (event->type)
    {
    case MODULE_EVENT_LOAD:
    {
        if (event->load.hid == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        MUTEX_SCOPE(&mutex);
        hid_handler_t* handler = module_hid_handler_lookup(event->load.hid);
        if (handler == NULL)
        {
            break;
        }

        if (!(handler->module->flags & MODULE_FLAG_LOADED))
        {
            if (module_load(handler->module) == ERR)
            {
                LOG_ERR("failed to load module for device '%s'\n", event->load.hid);
                break;
            }
        }
        assert(handler->module->procedure != NULL);

        if (handler->module->procedure(event) == ERR)
        {
            LOG_ERR("module procedure failed for device '%s\n", event->load.hid);
            break;
        }
    }
    break;
    default:
        return ERR;
    }*/
    return 0;
}

#include <kernel/acpi/acpi.h>
#include <kernel/acpi/aml/namespace.h>
#include <kernel/acpi/devices.h>

#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/aml/runtime/eisa_id.h>
#include <kernel/acpi/aml/runtime/evaluate.h>
#include <kernel/acpi/aml/runtime/method.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/acpi/resources.h>
#include <kernel/acpi/tables.h>
#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t acpi_id_object_to_string(aml_object_t* idObject, char* out, size_t outSize)
{
    if (idObject->type == AML_STRING)
    {
        size_t len = MIN(idObject->string.length, outSize - 1);
        strncpy_s(out, outSize, idObject->string.content, len);
        out[len] = '\0';
    }
    else if (idObject->type == AML_INTEGER)
    {
        if (aml_eisa_id_to_string(idObject->integer.value, out, outSize) == _FAIL)
        {
            return _FAIL;
        }
    }
    else
    {
        LOG_ERR("id object '%s' is of invalid type '%s'\n", AML_NAME_TO_STRING(idObject->name),
            aml_type_to_string(idObject->type));
        errno = EILSEQ;
        return _FAIL;
    }

    return 0;
}

static uint64_t acpi_sta_get_flags(aml_object_t* device, acpi_sta_flags_t* out)
{
    aml_object_t* sta = aml_namespace_find_child(NULL, device, AML_NAME('_', 'S', 'T', 'A'));
    if (sta == NULL)
    {
        *out = ACPI_STA_FLAGS_DEFAULT;
        return 0;
    }
    UNREF_DEFER(sta);

    aml_object_t* staResult = aml_evaluate(NULL, sta, AML_INTEGER);
    if (staResult == NULL)
    {
        LOG_ERR("failed to evaluate %s._STA\n", AML_NAME_TO_STRING(device->name));
        return _FAIL;
    }
    aml_uint_t value = staResult->integer.value;
    UNREF(staResult);

    if (value &
        ~(ACPI_STA_PRESENT | ACPI_STA_ENABLED | ACPI_STA_SHOW_IN_UI | ACPI_STA_FUNCTIONAL | ACPI_STA_BATTERY_PRESENT))
    {
        LOG_ERR("%s._STA returned invalid value 0x%llx\n", AML_NAME_TO_STRING(device->name), value);
        errno = EILSEQ;
        return _FAIL;
    }

    *out = (acpi_sta_flags_t)value;
    return 0;
}

typedef struct
{
    char hid[MAX_NAME];
    char cid[MAX_NAME];
    char path[MAX_PATH];
} acpi_id_t;

typedef struct
{
    acpi_id_t* array;
    uint64_t length;
} acpi_ids_t;

static uint64_t acpi_ids_push(acpi_ids_t* ids, const char* hid, const char* cid, const char* path)
{
    if (hid == NULL || hid[0] == '\0')
    {
        return 0;
    }

    acpi_id_t* newArray = realloc(ids->array, sizeof(acpi_id_t) * (ids->length + 1));
    if (newArray == NULL)
    {
        return _FAIL;
    }
    ids->array = newArray;

    strncpy_s(ids->array[ids->length].hid, MAX_NAME, hid, MAX_NAME);
    if (cid != NULL)
    {
        strncpy_s(ids->array[ids->length].cid, MAX_NAME, cid, MAX_NAME);
    }
    else
    {
        ids->array[ids->length].cid[0] = '\0';
    }
    strncpy_s(ids->array[ids->length].path, MAX_PATH, path, MAX_PATH);
    ids->length++;

    return 0;
}

static uint64_t acpi_ids_push_if_absent(acpi_ids_t* ids, const char* hid, const char* path)
{
    for (uint64_t i = 0; i < ids->length; i++)
    {
        if (strcmp(ids->array[i].hid, hid) == 0)
        {
            return 0;
        }
    }

    return acpi_ids_push(ids, hid, NULL, path);
}

static uint64_t acpi_ids_push_device(acpi_ids_t* ids, aml_object_t* device, const char* path)
{
    acpi_id_t deviceId = {0};

    aml_object_t* hid = aml_namespace_find_child(NULL, device, AML_NAME('_', 'H', 'I', 'D'));
    if (hid == NULL)
    {
        return 0; // Nothing to do
    }
    UNREF_DEFER(hid);

    aml_object_t* hidResult = aml_evaluate(NULL, hid, AML_STRING | AML_INTEGER);
    if (hidResult == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(hidResult);

    if (acpi_id_object_to_string(hidResult, deviceId.hid, MAX_NAME) == _FAIL)
    {
        return _FAIL;
    }

    if (strcmp(deviceId.hid, "ACPI0010") == 0) // Ignore Processor Container Device
    {
        return 0;
    }

    aml_object_t* cid = aml_namespace_find_child(NULL, device, AML_NAME('_', 'C', 'I', 'D'));
    if (cid != NULL)
    {
        UNREF_DEFER(cid);

        aml_object_t* cidResult = aml_evaluate(NULL, cid, AML_STRING | AML_INTEGER);
        if (cidResult == NULL)
        {
            return _FAIL;
        }
        UNREF_DEFER(cidResult);

        if (acpi_id_object_to_string(cidResult, deviceId.cid, MAX_NAME) == _FAIL)
        {
            return _FAIL;
        }
    }

    if (acpi_ids_push(ids, deviceId.hid, deviceId.cid, path) == _FAIL)
    {
        return _FAIL;
    }

    return 0;
}

static aml_object_t* acpi_sb_init(void)
{
    aml_object_t* sb = aml_namespace_find(NULL, NULL, 1, AML_NAME('_', 'S', 'B', '_'));
    if (sb == NULL) // Should never happen
    {
        LOG_ERR("failed to find \\_SB_ in namespace\n");
        return NULL;
    }
    UNREF_DEFER(sb);

    acpi_sta_flags_t sta;
    if (acpi_sta_get_flags(sb, &sta) == _FAIL)
    {
        return NULL;
    }

    if (!(sta & ACPI_STA_PRESENT))
    {
        LOG_INFO("\\_SB_ is not present, skipping initialization\n");
        return NULL;
    }

    aml_object_t* ini = aml_namespace_find_child(NULL, sb, AML_NAME('_', 'I', 'N', 'I'));
    if (ini != NULL)
    {
        UNREF_DEFER(ini);
        LOG_INFO("found \\_SB_._INI\n");
        aml_object_t* iniResult = aml_evaluate(NULL, ini, AML_ALL_TYPES);
        if (iniResult == NULL)
        {
            LOG_ERR("failed to evaluate \\_SB_._INI\n");
            return NULL;
        }
        UNREF(iniResult);
    }

    return REF(sb);
}

static uint64_t acpi_device_init_children(acpi_ids_t* ids, aml_object_t* device, const char* path)
{
    aml_object_t* child = NULL;
    LIST_FOR_EACH(child, &device->children, siblingsEntry)
    {
        if (child->type != AML_DEVICE)
        {
            continue;
        }

        char childPath[MAX_PATH];
        int written = snprintf(childPath, MAX_PATH, "%s.%s", path, AML_NAME_TO_STRING(child->name));
        if (written < 0 || written >= MAX_PATH)
        {
            LOG_ERR("ACPI path truncation for %s.%s, skipping deep nesting\n", path, AML_NAME_TO_STRING(child->name));
            continue;
        }

        acpi_sta_flags_t sta;
        if (acpi_sta_get_flags(child, &sta) == _FAIL)
        {
            return _FAIL;
        }

        if (sta & ACPI_STA_PRESENT)
        {
            aml_object_t* ini = aml_namespace_find_child(NULL, child, AML_NAME('_', 'I', 'N', 'I'));
            if (ini != NULL)
            {
                UNREF_DEFER(ini);
                aml_object_t* iniResult = aml_evaluate(NULL, ini, AML_ALL_TYPES);
                if (iniResult == NULL)
                {
                    LOG_ERR("failed to evaluate %s._INI\n", childPath);
                    return _FAIL;
                }
                UNREF(iniResult);
            }

            if (acpi_ids_push_device(ids, child, childPath) == _FAIL)
            {
                return _FAIL;
            }
        }

        if (sta & (ACPI_STA_PRESENT | ACPI_STA_FUNCTIONAL))
        {
            if (acpi_device_init_children(ids, child, childPath) == _FAIL)
            {
                return _FAIL;
            }
        }
    }
    return 0;
}

static int acpi_id_compare(const void* left, const void* right)
{
    const acpi_id_t* aPtr = (const acpi_id_t*)left;
    const acpi_id_t* bPtr = (const acpi_id_t*)right;

    const char* a = aPtr->hid;
    if (a[0] == '\0')
    {
        a = aPtr->cid;
    }

    const char* b = bPtr->hid;
    if (b[0] == '\0')
    {
        b = bPtr->cid;
    }

    // If alphabetic non-hex prefix differs, compare lexicographically
    while (*a && *b && ((*a < '0' || *a > '9') && (*a < 'A' || *a > 'F')) &&
        ((*b < '0' || *b > '9') && (*b < 'A' || *b > 'F')))
    {
        if (*a != *b)
        {
            return (unsigned char)*a - (unsigned char)*b;
        }
        a++;
        b++;
    }

    // If alphabetic prefix is the same, compare hex numerically
    uint64_t numA = 0;
    sscanf(a, "%llx", &numA);
    uint64_t numB = 0;
    sscanf(b, "%llx", &numB);
    return numA - numB;
}

static void acpi_device_cfg_free(acpi_device_cfg_t* cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    for (uint64_t i = 0; i < cfg->irqCount; i++)
    {
        irq_virt_free(cfg->irqs[i].virt);
    }

    for (uint64_t i = 0; i < cfg->ioCount; i++)
    {
        port_release(cfg->ios[i].base, cfg->ios[i].length);
    }

    free(cfg->irqs);
    free(cfg->ios);
    free(cfg);
}

static uint64_t acpi_device_configure(const char* name)
{
    if (name[0] == '.')
    {
        LOG_DEBUG("skipping configuration for fake ACPI device '%s'\n", name);
        return 0;
    }

    aml_object_t* device = aml_namespace_find_by_path(NULL, NULL, name);
    if (device == NULL)
    {
        LOG_ERR("failed to find ACPI device '%s' in namespace for configuration\n", name);
        return _FAIL;
    }
    UNREF_DEFER(device);

    if (device->type != AML_DEVICE)
    {
        LOG_ERR("ACPI object '%s' is not a device, cannot configure\n", name);
        errno = EINVAL;
        return _FAIL;
    }

    if (device->device.cfg != NULL)
    {
        LOG_DEBUG("ACPI device '%s' is already configured, skipping\n", name);
        return 0;
    }

    acpi_device_cfg_t* cfg = calloc(1, sizeof(acpi_device_cfg_t));
    if (cfg == NULL)
    {
        return _FAIL;
    }

    acpi_resources_t* resources = acpi_resources_current(device);
    if (resources == NULL)
    {
        if (errno == ENOENT) // No resources exist, assign empty config
        {
            device->device.cfg = cfg;
            return 0;
        }
        LOG_ERR("failed to get current resources for ACPI device '%s' due to '%s'\n", name, strerror(errno));
        free(cfg);
        return _FAIL;
    }

    acpi_resource_t* resource;
    ACPI_RESOURCES_FOR_EACH(resource, resources)
    {
        acpi_item_name_t itemName = ACPI_RESOURCE_ITEM_NAME(resource);
        switch (itemName)
        {
        case ACPI_ITEM_NAME_IRQ:
        {
            acpi_irq_descriptor_t* desc = (acpi_irq_descriptor_t*)resource;
            acpi_irq_descriptor_info_t info = ACPI_IRQ_DESCRIPTOR_INFO(desc);

            irq_flags_t flags = ((info & ACPI_IRQ_EDGE_TRIGGERED) ? IRQ_TRIGGER_EDGE : IRQ_TRIGGER_LEVEL) |
                ((info & ACPI_IRQ_ACTIVE_LOW) ? IRQ_POLARITY_LOW : IRQ_POLARITY_HIGH) |
                ((info & ACPI_IRQ_EXCLUSIVE) ? IRQ_EXCLUSIVE : IRQ_SHARED);

            for (irq_phys_t phys = 0; phys < 16; phys++)
            {
                if (!((desc->mask >> phys) & 1))
                {
                    continue;
                }

                irq_virt_t virt;
                if (irq_virt_alloc(&virt, phys, flags, NULL) == _FAIL)
                {
                    LOG_ERR("failed to allocate virtual IRQ for ACPI device '%s' due to '%s'\n", name, strerror(errno));
                    goto error;
                }

                acpi_device_irq_t* newIrqs = realloc(cfg->irqs, sizeof(acpi_device_irq_t) * (cfg->irqCount + 1));
                if (newIrqs == NULL)
                {
                    LOG_ERR("failed to allocate memory for IRQs for ACPI device '%s' due to '%s'\n", name,
                        strerror(errno));
                    irq_virt_free(virt);
                    goto error;
                }
                cfg->irqs = newIrqs;
                cfg->irqs[cfg->irqCount].phys = phys;
                cfg->irqs[cfg->irqCount].virt = virt;
                cfg->irqs[cfg->irqCount].flags = flags;
                cfg->irqCount++;
            }
        }
        break;
        case ACPI_ITEM_NAME_IO_PORT:
        {
            acpi_io_port_descriptor_t* desc = (acpi_io_port_descriptor_t*)resource;

            acpi_device_io_t* newIos = realloc(cfg->ios, sizeof(acpi_device_io_t) * (cfg->ioCount + 1));
            if (newIos == NULL)
            {
                LOG_ERR("failed to allocate memory for IO ports for ACPI device '%s' due to '%s'\n", name,
                    strerror(errno));
                goto error;
            }
            cfg->ios = newIos;

            port_t base;
            if (port_reserve(&base, desc->minBase, desc->maxBase, desc->alignment, desc->length, name) == _FAIL)
            {
                LOG_ERR("failed to reserve IO ports for ACPI device '%s' due to '%s'\n", name, strerror(errno));
                goto error;
            }

            cfg->ios[cfg->ioCount].base = base;
            cfg->ios[cfg->ioCount].length = desc->length;
            cfg->ioCount++;
        }
        break;
        default:
            break;
        }
    }

    free(resources);
    device->device.cfg = cfg;
    return 0;

error:
    free(resources);
    acpi_device_cfg_free(cfg);
    return _FAIL;
}

uint64_t acpi_devices_init(void)
{
    MUTEX_SCOPE(aml_big_mutex_get());

    acpi_ids_t ids = {
        .array = NULL,
        .length = 0,
    };

    aml_object_t* sb = acpi_sb_init();
    if (sb == NULL)
    {
        LOG_ERR("failed to initialize ACPI devices\n");
        return _FAIL;
    }
    UNREF_DEFER(sb);

    LOG_DEBUG("initializing ACPI devices under \\_SB_\n");
    if (acpi_device_init_children(&ids, sb, "\\_SB_") == _FAIL)
    {
        LOG_ERR("failed to initialize ACPI devices\n");
        return _FAIL;
    }

    // Because of... reasons some hardware wont report certain devices via ACPI
    // even if they actually do have it. In these cases we manually add their HIDs.

    if (acpi_tables_lookup("HPET", sizeof(sdt_header_t), 0) != NULL) // HPET
    {
        if (acpi_ids_push_if_absent(&ids, "PNP0103", ".HPET") == _FAIL)
        {
            LOG_ERR("failed to initialize ACPI devices\n");
            return _FAIL;
        }
    }

    if (acpi_tables_lookup("APIC", sizeof(sdt_header_t), 0) != NULL) // APIC
    {
        if (acpi_ids_push_if_absent(&ids, "PNP0003", ".APIC") == _FAIL)
        {
            LOG_ERR("failed to initialize ACPI devices\n");
            return _FAIL;
        }
    }

    if (ids.array != NULL && ids.length > 0)
    {
        qsort(ids.array, ids.length, sizeof(acpi_id_t), acpi_id_compare);
    }

    for (size_t i = 0; i < ids.length; i++)
    {
        if (acpi_device_configure(ids.array[i].path) == _FAIL)
        {
            // Dont load module for unconfigurable device
            memmove(&ids.array[i], &ids.array[i + 1], sizeof(acpi_id_t) * (ids.length - i - 1));
            ids.length--;
            i--;
            continue;
        }
    }

    for (size_t i = 0; i < ids.length; i++)
    {
        uint64_t loadedModules = module_device_attach(ids.array[i].hid, ids.array[i].path, MODULE_LOAD_ONE);
        if (loadedModules == _FAIL)
        {
            LOG_ERR("failed to load module for HID '%s' due to '%s'\n", ids.array[i].hid, strerror(errno));
            continue;
        }

        if (loadedModules != 0 || ids.array[i].cid[0] == '\0')
        {
            continue;
        }

        loadedModules = module_device_attach(ids.array[i].cid, ids.array[i].path, MODULE_LOAD_ONE);
        if (loadedModules == _FAIL)
        {
            LOG_ERR("failed to load module for CID '%s' due to '%s'\n", ids.array[i].cid, strerror(errno));
        }
    }

    free(ids.array);
    return 0;
}

acpi_device_cfg_t* acpi_device_cfg_lookup(const char* name)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    MUTEX_SCOPE(aml_big_mutex_get());

    aml_object_t* device = aml_namespace_find_by_path(NULL, NULL, name);
    if (device == NULL)
    {
        LOG_ERR("failed to find ACPI device '%s' in namespace for configuration retrieval\n", name);
        errno = ENOENT;
        return NULL;
    }
    UNREF_DEFER(device);

    if (device->type != AML_DEVICE)
    {
        LOG_ERR("ACPI object '%s' is not a device, cannot retrieve configuration\n", name);
        errno = ENOTTY;
        return NULL;
    }

    if (device->device.cfg == NULL)
    {
        LOG_ERR("ACPI device '%s' is not configured\n", name);
        errno = ENODEV;
        return NULL;
    }

    return device->device.cfg;
}

uint64_t acpi_device_cfg_get_port(acpi_device_cfg_t* cfg, uint64_t index, port_t* out)
{
    if (cfg == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    for (uint64_t i = 0; i < cfg->ioCount; i++)
    {
        if (cfg->ios[i].length > index)
        {
            *out = cfg->ios[i].base + index;
            return 0;
        }
        index -= cfg->ios[i].length;
    }

    errno = ENOSPC;
    return _FAIL;
}
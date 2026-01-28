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
#include <kernel/utils/ref.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/status.h>

static status_t acpi_id_object_to_string(aml_object_t* idObject, char* out, size_t outSize)
{
    if (idObject->type == AML_STRING)
    {
        size_t len = MIN(idObject->string.length, outSize - 1);
        strncpy_s(out, outSize, idObject->string.content, len);
        out[len] = '\0';
    }
    else if (idObject->type == AML_INTEGER)
    {
        status_t status = aml_eisa_id_to_string(idObject->integer.value, out, outSize);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    else
    {
        LOG_ERR("id object '%s' is of invalid type '%s'\n", AML_NAME_TO_STRING(idObject->name),
            aml_type_to_string(idObject->type));
        return ERR(ACPI, ILSEQ);
    }

    return OK;
}

static status_t acpi_sta_get_flags(aml_object_t* device, acpi_sta_flags_t* out)
{
    aml_object_t* sta = aml_namespace_find_child(NULL, device, AML_NAME('_', 'S', 'T', 'A'));
    if (sta == NULL)
    {
        *out = ACPI_STA_FLAGS_DEFAULT;
        return OK;
    }
    UNREF_DEFER(sta);

    aml_object_t* staResult = NULL;
    status_t status = aml_evaluate(NULL, sta, AML_INTEGER, &staResult);
    if (IS_ERR(status))
    {
        LOG_ERR("failed to evaluate %s._STA\n", AML_NAME_TO_STRING(device->name));
        return status;
    }
    aml_uint_t value = staResult->integer.value;
    UNREF(staResult);

    if (value &
        ~(ACPI_STA_PRESENT | ACPI_STA_ENABLED | ACPI_STA_SHOW_IN_UI | ACPI_STA_FUNCTIONAL | ACPI_STA_BATTERY_PRESENT))
    {
        LOG_ERR("%s._STA returned invalid value 0x%llx\n", AML_NAME_TO_STRING(device->name), value);
        return ERR(ACPI, ILSEQ);
    }

    *out = (acpi_sta_flags_t)value;
    return OK;
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

static status_t acpi_ids_push(acpi_ids_t* ids, const char* hid, const char* cid, const char* path)
{
    if (hid == NULL || hid[0] == '\0')
    {
        return OK;
    }

    acpi_id_t* newArray = realloc(ids->array, sizeof(acpi_id_t) * (ids->length + 1));
    if (newArray == NULL)
    {
        return ERR(ACPI, NOMEM);
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

    return OK;
}

static status_t acpi_ids_push_if_absent(acpi_ids_t* ids, const char* hid, const char* path)
{
    for (uint64_t i = 0; i < ids->length; i++)
    {
        if (strcmp(ids->array[i].hid, hid) == 0)
        {
            return OK;
        }
    }

    return acpi_ids_push(ids, hid, NULL, path);
}

static status_t acpi_ids_push_device(acpi_ids_t* ids, aml_object_t* device, const char* path)
{
    acpi_id_t deviceId = {0};

    aml_object_t* hid = aml_namespace_find_child(NULL, device, AML_NAME('_', 'H', 'I', 'D'));
    if (hid == NULL)
    {
        return OK; // Nothing to do
    }
    UNREF_DEFER(hid);

    aml_object_t* hidResult = NULL;
    status_t status = aml_evaluate(NULL, hid, AML_STRING | AML_INTEGER, &hidResult);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(hidResult);

    status = acpi_id_object_to_string(hidResult, deviceId.hid, MAX_NAME);
    if (IS_ERR(status))
    {
        return status;
    }

    if (strcmp(deviceId.hid, "ACPI0010") == 0) // Ignore Processor Container Device
    {
        return OK;
    }

    aml_object_t* cid = aml_namespace_find_child(NULL, device, AML_NAME('_', 'C', 'I', 'D'));
    if (cid != NULL)
    {
        UNREF_DEFER(cid);

        aml_object_t* cidResult = NULL;
        status = aml_evaluate(NULL, cid, AML_STRING | AML_INTEGER, &cidResult);
        if (IS_ERR(status))
        {
            return status;
        }
        UNREF_DEFER(cidResult);

        status = acpi_id_object_to_string(cidResult, deviceId.cid, MAX_NAME);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    status = acpi_ids_push(ids, deviceId.hid, deviceId.cid, path);
    if (IS_ERR(status))
    {
        return status;
    }

    return OK;
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
    if (IS_ERR(acpi_sta_get_flags(sb, &sta)))
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
        aml_object_t* iniResult = NULL;
        status_t status = aml_evaluate(NULL, ini, AML_ALL_TYPES, &iniResult);
        if (IS_ERR(status))
        {
            LOG_ERR("failed to evaluate \\_SB_._INI\n");
            return NULL;
        }
        UNREF(iniResult);
    }

    return REF(sb);
}

static status_t acpi_device_init_children(acpi_ids_t* ids, aml_object_t* device, const char* path)
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
        status_t status = acpi_sta_get_flags(child, &sta);
        if (IS_ERR(status))
        {
            return status;
        }

        if (sta & ACPI_STA_PRESENT)
        {
            aml_object_t* ini = aml_namespace_find_child(NULL, child, AML_NAME('_', 'I', 'N', 'I'));
            if (ini != NULL)
            {
                UNREF_DEFER(ini);
                aml_object_t* iniResult = NULL;
                status = aml_evaluate(NULL, ini, AML_ALL_TYPES, &iniResult);
                if (IS_ERR(status))
                {
                    LOG_ERR("failed to evaluate %s._INI\n", childPath);
                    return status;
                }
                UNREF(iniResult);
            }

            status = acpi_ids_push_device(ids, child, childPath);
            if (IS_ERR(status))
            {
                return status;
            }
        }

        if (sta & (ACPI_STA_PRESENT | ACPI_STA_FUNCTIONAL))
        {
            status = acpi_device_init_children(ids, child, childPath);
            if (IS_ERR(status))
            {
                return status;
            }
        }
    }
    return OK;
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

static void acpi_dev_free(acpi_dev_t* cfg)
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

static status_t acpi_device_configure(const char* name)
{
    if (name[0] == '.')
    {
        LOG_DEBUG("skipping configuration for fake ACPI device '%s'\n", name);
        return OK;
    }

    aml_object_t* device = aml_namespace_find_by_path(NULL, NULL, name);
    if (device == NULL)
    {
        LOG_ERR("failed to find ACPI device '%s' in namespace for configuration\n", name);
        return ERR(ACPI, NOENT);
    }
    UNREF_DEFER(device);

    if (device->type != AML_DEVICE)
    {
        LOG_ERR("ACPI object '%s' is not a device, cannot configure\n", name);
        return ERR(ACPI, INVAL);
    }

    if (device->device.cfg != NULL)
    {
        LOG_DEBUG("ACPI device '%s' is already configured, skipping\n", name);
        return OK;
    }

    acpi_dev_t* cfg = calloc(1, sizeof(acpi_dev_t));
    if (cfg == NULL)
    {
        return ERR(ACPI, NOMEM);
    }

    acpi_resources_t* resources = NULL;
    status_t status = acpi_resources_current(device, &resources);
    if (IS_ERR(status))
    {
        if (ST_CODE(status) == ST_CODE_NOENT) // No resources exist, assign empty config
        {
            device->device.cfg = cfg;
            return OK;
        }
        LOG_ERR("failed to get current resources for ACPI device '%s' due to '%s'\n", name, codetostr(ST_CODE(status)));
        free(cfg);
        return status;
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
                status = irq_virt_alloc(&virt, phys, flags, NULL);
                if (IS_ERR(status))
                {
                    LOG_ERR("failed to allocate virtual IRQ for ACPI device '%s' due to '%s'\n", name, codetostr(ST_CODE(status)));
                    goto error;
                }

                acpi_device_irq_t* newIrqs = realloc(cfg->irqs, sizeof(acpi_device_irq_t) * (cfg->irqCount + 1));
                if (newIrqs == NULL)
                {
                    LOG_ERR("failed to allocate memory for IRQs for ACPI device '%s'\n", name);
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
                LOG_ERR("failed to allocate memory for IO ports for ACPI device '%s'\n", name);
                goto error;
            }
            cfg->ios = newIos;

            port_t base;
            status = port_reserve(&base, desc->minBase, desc->maxBase, desc->alignment, desc->length, name);
            if (IS_ERR(status))
            {
                LOG_ERR("failed to reserve IO ports for ACPI device '%s' due to '%s'\n", name, codetostr(ST_CODE(status)));
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
    return OK;

error:
    free(resources);
    acpi_dev_free(cfg);
    return status;
}

status_t acpi_devices_init(void)
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
        return ERR(ACPI, NOENT);
    }
    UNREF_DEFER(sb);

    LOG_DEBUG("initializing ACPI devices under \\_SB_\n");
    status_t status = acpi_device_init_children(&ids, sb, "\\_SB_");
    if (IS_ERR(status))
    {
        LOG_ERR("failed to initialize ACPI devices\n");
        return status;
    }

    // Because of... reasons some hardware wont report certain devices via ACPI
    // even if they actually do have it. In these cases we manually add their HIDs.

    if (acpi_tables_lookup("HPET", sizeof(sdt_header_t), 0) != NULL) // HPET
    {
        status = acpi_ids_push_if_absent(&ids, "PNP0103", ".HPET");
        if (IS_ERR(status))
        {
            LOG_ERR("failed to initialize ACPI devices\n");
            return status;
        }
    }

    if (acpi_tables_lookup("APIC", sizeof(sdt_header_t), 0) != NULL) // APIC
    {
        status = acpi_ids_push_if_absent(&ids, "PNP0003", ".APIC");
        if (IS_ERR(status))
        {
            LOG_ERR("failed to initialize ACPI devices\n");
            return status;
        }
    }

    if (ids.array != NULL && ids.length > 0)
    {
        qsort(ids.array, ids.length, sizeof(acpi_id_t), acpi_id_compare);
    }

    for (size_t i = 0; i < ids.length; i++)
    {
        if (IS_ERR(acpi_device_configure(ids.array[i].path)))
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
        uint64_t loadedModules;
        status_t status = module_device_attach(ids.array[i].hid, ids.array[i].path, MODULE_LOAD_ONE, &loadedModules);
        if (IS_ERR(status))
        {
            LOG_ERR("failed to load module for HID '%s' due to '%s'\n", ids.array[i].hid, codetostr(status));
            continue;
        }

        if (loadedModules != 0 || ids.array[i].cid[0] == '\0')
        {
            continue;
        }

        status = module_device_attach(ids.array[i].cid, ids.array[i].path, MODULE_LOAD_ONE, NULL);
        if (IS_ERR(status))
        {
            LOG_ERR("failed to load module for CID '%s' due to '%s'\n", ids.array[i].cid, codetostr(status));
        }
    }

    free(ids.array);
    return OK;
}

status_t acpi_dev_lookup(const char* name, acpi_dev_t** out)
{
    if (name == NULL || out == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    MUTEX_SCOPE(aml_big_mutex_get());

    aml_object_t* device = aml_namespace_find_by_path(NULL, NULL, name);
    if (device == NULL)
    {
        LOG_ERR("failed to find ACPI device '%s' in namespace for configuration retrieval\n", name);
        return ERR(ACPI, NOENT);
    }
    UNREF_DEFER(device);

    if (device->type != AML_DEVICE)
    {
        LOG_ERR("ACPI object '%s' is not a device, cannot retrieve configuration\n", name);
        return ERR(ACPI, NOTTY);
    }

    if (device->device.cfg == NULL)
    {
        LOG_ERR("ACPI device '%s' is not configured\n", name);
        return ERR(ACPI, NODEV);
    }

    *out = device->device.cfg;
    return OK;
}

status_t acpi_dev_get_port(acpi_dev_t* cfg, uint64_t index, port_t* out)
{
    if (cfg == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    for (uint64_t i = 0; i < cfg->ioCount; i++)
    {
        if (cfg->ios[i].length > index)
        {
            *out = cfg->ios[i].base + index;
            return OK;
        }
        index -= cfg->ios[i].length;
    }

    return ERR(ACPI, NOSPACE);
}
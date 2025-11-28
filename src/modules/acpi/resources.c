#include <modules/acpi/aml/namespace.h>
#include <modules/acpi/resources.h>

#include <modules/acpi/acpi.h>
#include <modules/acpi/devices.h>

#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/utils/ref.h>
#include <modules/acpi/aml/aml.h>
#include <modules/acpi/aml/object.h>
#include <modules/acpi/aml/runtime/eisa_id.h>
#include <modules/acpi/aml/runtime/evaluate.h>
#include <modules/acpi/aml/runtime/method.h>
#include <modules/acpi/aml/state.h>
#include <modules/acpi/aml/to_string.h>
#include <modules/acpi/tables.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

acpi_resources_t* acpi_resources_current(aml_object_t* device)
{
    if (device == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    aml_object_t* crs = aml_namespace_find_child(NULL, device, AML_NAME('_', 'C', 'R', 'S'));
    if (crs == NULL)
    {
        errno = ENOENT;
        return NULL;
    }
    DEREF_DEFER(crs);

    aml_object_t* crsResult = aml_evaluate(NULL, crs, AML_BUFFER);
    if (crsResult == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(crsResult);

    if (crsResult->type != AML_BUFFER)
    {
        errno = EILSEQ;
        return NULL;
    }

    acpi_resources_t* resources = malloc(sizeof(acpi_resources_t) + crsResult->buffer.length);
    if (resources == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    resources->length = crsResult->buffer.length;
    memcpy_s(resources->data, crsResult->buffer.length, crsResult->buffer.content, crsResult->buffer.length);

    bool endTagFound = false;
    acpi_resource_t* resource;
    ACPI_RESOURCES_FOR_EACH(resource, resources)
    {
        if (endTagFound)
        {
            goto error;
        }

        acpi_item_name_t itemName = ACPI_RESOURCE_ITEM_NAME(resource);
        uint64_t size = ACPI_RESOURCE_SIZE(resource);

        switch (itemName)
        {
        case ACPI_ITEM_NAME_IRQ:
        {
            if (size != sizeof(acpi_irq_descriptor_t) &&
                size != sizeof(acpi_irq_descriptor_t) + 1) // The last byte is optional
            {
                goto error;
            }
        }
        break;
        case ACPI_ITEM_NAME_IO_PORT:
        {
            if (size != sizeof(acpi_io_port_descriptor_t))
            {
                goto error;
            }
        }
        break;
        case ACPI_ITEM_NAME_END_TAG:
            endTagFound = true;
            break;
        default:
            break;
        }
    }

    if (!endTagFound)
    {
        LOG_ERR("device '%s' _CRS missing end tag\n", AML_NAME_TO_STRING(device->name));
        goto error;
    }

    return resources;
error:
    free(resources);
    errno = EILSEQ;
    return NULL;
}

void acpi_resources_free(acpi_resources_t* resources)
{
    free(resources);
}
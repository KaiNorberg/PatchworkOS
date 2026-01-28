#include <kernel/acpi/aml/namespace.h>
#include <kernel/acpi/resources.h>

#include <kernel/acpi/acpi.h>
#include <kernel/acpi/devices.h>

#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/aml/runtime/eisa_id.h>
#include <kernel/acpi/aml/runtime/evaluate.h>
#include <kernel/acpi/aml/runtime/method.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/acpi/tables.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/utils/ref.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

status_t acpi_resources_current(aml_object_t* device, acpi_resources_t** out)
{
    if (device == NULL || out == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    aml_object_t* crs = aml_namespace_find_child(NULL, device, AML_NAME('_', 'C', 'R', 'S'));
    if (crs == NULL)
    {
        return ERR(ACPI, NOENT);
    }
    UNREF_DEFER(crs);

    aml_object_t* crsResult = NULL;
    status_t status = aml_evaluate(NULL, crs, AML_BUFFER, &crsResult);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(crsResult);

    if (crsResult->type != AML_BUFFER)
    {
        return ERR(ACPI, ILSEQ);
    }

    acpi_resources_t* resources = malloc(sizeof(acpi_resources_t) + crsResult->buffer.length);
    if (resources == NULL)
    {
        return ERR(ACPI, NOMEM);
    }

    resources->length = crsResult->buffer.length;
    memcpy_s(resources->data, crsResult->buffer.length, crsResult->buffer.content, crsResult->buffer.length);

    bool endTagFound = false;
    acpi_resource_t* resource;
    ACPI_RESOURCES_FOR_EACH(resource, resources)
    {
        if (endTagFound)
        {
            status = ERR(ACPI, ILSEQ);
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
                status = ERR(ACPI, ILSEQ);
                goto error;
            }
        }
        break;
        case ACPI_ITEM_NAME_IO_PORT:
        {
            if (size != sizeof(acpi_io_port_descriptor_t))
            {
                status = ERR(ACPI, ILSEQ);
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
        status = ERR(ACPI, ILSEQ);
        goto error;
    }

    *out = resources;
    return OK;
error:
    free(resources);
    return status;
}

void acpi_resources_free(acpi_resources_t* resources)
{
    free(resources);
}
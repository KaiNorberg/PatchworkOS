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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

acpi_resources_t* acpi_resources_current(const char* path)
{
    if (path == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    aml_object_t* device = aml_namespace_find_by_path(NULL, NULL, path);
    if (device == NULL)
    {
        LOG_ERR("device '%s' not found\n", path);
        errno = ENODEV;
        return NULL;
    }
    DEREF_DEFER(device);

    aml_object_t* crs = aml_namespace_find_child(NULL, device, AML_NAME('_', 'C', 'R', 'S'));
    if (crs == NULL)
    {
        LOG_ERR("device '%s' has no _CRS method\n", path);
        errno = ENODEV;
        return NULL;
    }
    DEREF_DEFER(crs);

    aml_object_t* crsResult = aml_evaluate(NULL, crs, AML_BUFFER);
    if (crsResult == NULL)
    {
        LOG_ERR("could not evaluate %s._CRS\n", path);
        return NULL;
    }
    DEREF_DEFER(crsResult);

    if (crsResult->type != AML_BUFFER)
    {
        LOG_ERR("device '%s' _CRS did not return a buffer\n", path);
        errno = EILSEQ;
        return NULL;
    }

    acpi_resources_t* resources = (acpi_resources_t*)malloc(sizeof(acpi_resources_t) + crsResult->buffer.length);
    if (resources == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    resources->length = crsResult->buffer.length;
    memcpy_s(resources->data, crsResult->buffer.length, crsResult->buffer.content, crsResult->buffer.length);

    return resources;
}
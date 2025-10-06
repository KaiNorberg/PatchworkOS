#include "devices.h"

#include "aml/aml.h"
#include "aml/aml_object.h"
#include "aml/aml_to_string.h"
#include "aml/runtime/convert.h"
#include "aml/runtime/method.h"
#include "log/log.h"

static inline uint64_t acpi_sta_get_flags(aml_object_t* device, acpi_sta_flags_t* out)
{
    aml_object_t* sta = aml_object_find_child(device, "_STA");
    if (sta == NULL)
    {
        *out = ACPI_STA_FLAGS_DEFAULT;
        return 0;
    }

    uint64_t value;
    if (aml_method_evaluate_integer(sta, &value) == ERR)
    {
        LOG_ERR("could not evaluate %s._STA\n", AML_OBJECT_GET_NAME(device));
        return ERR;
    }

    if (value &
        ~(ACPI_STA_PRESENT | ACPI_STA_ENABLED | ACPI_STA_SHOW_IN_UI | ACPI_STA_FUNCTIONAL | ACPI_STA_BATTERY_PRESENT))
    {
        LOG_ERR("%s._STA returned invalid value 0x%llx\n", AML_OBJECT_GET_NAME(device), value);
        errno = EILSEQ;
        return ERR;
    }

    *out = (acpi_sta_flags_t)value;
    return 0;
}

static inline uint64_t acpi_devices_init_children(aml_object_t* parent)
{
    if (parent->type != AML_DEVICE)
    {
        return 0; // Nothing to do
    }

    aml_object_t* child;
    LIST_FOR_EACH(child, &parent->device.namedObjects, name.entry)
    {
        if (child->type != AML_DEVICE)
        {
            continue; // Only devices can have _STA and _INI
        }

        acpi_sta_flags_t sta;
        if (acpi_sta_get_flags(child, &sta) == ERR)
        {
            return ERR;
        }

        if (sta & ACPI_STA_PRESENT)
        {
            aml_object_t* ini = aml_object_find_child(child, "_INI");
            if (ini != NULL)
            {
                if (ini->type != AML_METHOD)
                {
                    LOG_ERR("%s._INI is a '%s', not a method\n", AML_OBJECT_GET_NAME(child),
                        aml_type_to_string(ini->type));
                    return ERR;
                }

                LOG_INFO("ACPI device '%s._INI'\n", AML_OBJECT_GET_NAME(child));
                if (aml_method_evaluate(&ini->method, 0, NULL, NULL) == ERR)
                {
                    LOG_ERR("could not evaluate %s._INI\n", AML_OBJECT_GET_NAME(child));
                    return ERR;
                }
            }
        }

        if (sta & (ACPI_STA_PRESENT | ACPI_STA_FUNCTIONAL))
        {
            if (acpi_devices_init_children(child) == ERR)
            {
                return ERR;
            }
        }
    }

    return 0;
}

uint64_t acpi_devices_init(void)
{
    // TODO: Implement all opcodes needed for _INI and device initalization
    MUTEX_SCOPE(aml_big_mutex_get());

    aml_object_t* sbIni = aml_object_find(NULL, "\\_SB_._INI");
    if (sbIni != NULL)
    {
        if (sbIni->type != AML_METHOD)
        {
            LOG_ERR("\\_SB_._INI is a '%s', not a method\n", aml_type_to_string(sbIni->type));
            return ERR;
        }

        LOG_INFO("found \\_SB_._INI\n");
        if (aml_method_evaluate(&sbIni->method, 0, NULL, NULL) == ERR)
        {
            return ERR;
        }
    }

    aml_object_t* sb = aml_object_find(NULL, "\\_SB_");
    if (sb == NULL) // Should never happen
    {
        LOG_ERR("could not find \\_SB_\n");
        return ERR;
    }

    if (acpi_devices_init_children(sb) == ERR)
    {
        return ERR;
    }

    return 0;
}

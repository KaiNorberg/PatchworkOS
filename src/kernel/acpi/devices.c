#include "devices.h"

#include "aml/aml_node.h"
#include "aml/runtime/convert.h"
#include "aml/runtime/method.h"
#include "log/log.h"

static inline uint64_t acpi_sta_get_flags(aml_node_t* device, acpi_sta_flags_t* out)
{
    aml_node_t* sta = aml_node_find_child(device, "_STA");
    if (sta == NULL)
    {
        *out = ACPI_STA_FLAGS_DEFAULT;
        return 0;
    }

    uint64_t value;
    if (aml_method_evaluate_integer(sta, &value) == ERR)
    {
        LOG_ERR("could not evaluate %s._STA\n", device->segment);
        return ERR;
    }

    *out = (acpi_sta_flags_t)value;
    return 0;
}

static inline uint64_t acpi_devices_init_children(aml_node_t* parent)
{
    aml_node_t* child;
    LIST_FOR_EACH(child, &parent->children, entry)
    {
        if (child->type == AML_DATA_DEVICE)
        {
            acpi_sta_flags_t sta;
            if (acpi_sta_get_flags(child, &sta) == ERR)
            {
                return ERR;
            }

            if (sta & ACPI_STA_PRESENT)
            {
                aml_node_t* ini = aml_node_find_child(child, "_INI");
                if (ini != NULL)
                {
                    LOG_INFO("initalizing '%.*s'\n", AML_NAME_LENGTH, child->segment);
                    if (aml_method_evaluate(ini, NULL, NULL) == ERR)
                    {
                        LOG_ERR("could not evaluate %s._INI\n", child->segment);
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
    }

    return 0;
}

uint64_t acpi_devices_init(void)
{
    aml_node_t* sbIni = aml_node_find(NULL, "\\_SB._INI");
    if (sbIni != NULL)
    {
        LOG_INFO("found \\_SB._INI\n");
        if (aml_method_evaluate(sbIni, NULL, NULL) == ERR)
        {
            return ERR;
        }
    }

    aml_node_t* sb = aml_node_find(NULL, "\\_SB");
    if (sb == NULL) // Should never happen
    {
        LOG_ERR("could not find \\_SB\n");
        return ERR;
    }

    if (acpi_devices_init_children(sb) == ERR)
    {
        return ERR;
    }

    return 0;
}

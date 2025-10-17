#include "devices.h"

#include "aml/aml.h"
#include "aml/object.h"
#include "aml/runtime/method.h"
#include "aml/state.h"
#include "aml/to_string.h"
#include "log/log.h"
#include "log/panic.h"

#include <errno.h>

static inline uint64_t acpi_sta_get_flags(aml_state_t* state, aml_object_t* device, acpi_sta_flags_t* out)
{
    aml_object_t* sta = aml_namespace_find(&state->overlay, device, AML_NAME('_', 'S', 'T', 'A'));
    if (sta == NULL)
    {
        *out = ACPI_STA_FLAGS_DEFAULT;
        return 0;
    }
    DEREF_DEFER(sta);

    aml_integer_t value;
    if (aml_method_evaluate_integer(state, sta, &value) == ERR)
    {
        LOG_ERR("could not evaluate %s._STA\n", AML_NAME_TO_STRING(device->name));
        return ERR;
    }

    if (value &
        ~(ACPI_STA_PRESENT | ACPI_STA_ENABLED | ACPI_STA_SHOW_IN_UI | ACPI_STA_FUNCTIONAL | ACPI_STA_BATTERY_PRESENT))
    {
        LOG_ERR("%s._STA returned invalid value 0x%llx\n", AML_NAME_TO_STRING(device->name), value);
        errno = EILSEQ;
        return ERR;
    }

    *out = (acpi_sta_flags_t)value;
    return 0;
}

static inline uint64_t acpi_devices_init_children(aml_state_t* state, aml_object_t* parent)
{
    if (parent->type != AML_DEVICE)
    {
        return 0; // Nothing to do
    }

    aml_object_t* child;
    LIST_FOR_EACH(child, &parent->children, siblingsEntry)
    {
        if (child->type != AML_DEVICE)
        {
            continue; // Only devices can have _STA and _INI
        }

        acpi_sta_flags_t sta;
        if (acpi_sta_get_flags(state, child, &sta) == ERR)
        {
            return ERR;
        }

        if (sta & ACPI_STA_PRESENT)
        {
            aml_object_t* ini = aml_namespace_find(NULL, child, AML_NAME('_', 'I', 'N', 'I'));
            if (ini != NULL)
            {
                DEREF_DEFER(ini);

                if (ini->type != AML_METHOD)
                {
                    LOG_ERR("%s._INI is a '%s', not a method\n", AML_NAME_TO_STRING(child->name),
                        aml_type_to_string(ini->type));
                    return ERR;
                }

                LOG_INFO("ACPI device '%s._INI'\n", AML_NAME_TO_STRING(child->name));
                if (aml_method_evaluate_integer(state, ini, NULL) == ERR)
                {
                    LOG_ERR("could not evaluate %s._INI\n", AML_NAME_TO_STRING(child->name));
                    return ERR;
                }
            }
        }

        if (sta & (ACPI_STA_PRESENT | ACPI_STA_FUNCTIONAL))
        {
            if (acpi_devices_init_children(state, child) == ERR)
            {
                return ERR;
            }
        }
    }

    return 0;
}

void acpi_devices_init(void)
{
    MUTEX_SCOPE(aml_big_mutex_get());

    aml_state_t state;
    if (aml_state_init(&state, NULL) == ERR)
    {
        panic(NULL, "could not initialize AML state for ACPI device initialization\n");
    }

    aml_object_t* sb = aml_namespace_find(NULL, NULL, 1, AML_NAME('_', 'S', 'B', '_'));
    if (sb == NULL) // Should never happen
    {
        aml_state_deinit(&state);
        LOG_ERR("could not find \\_SB_ in namespace\n");
    }
    DEREF_DEFER(sb);

    aml_object_t* sbIni = aml_namespace_find_child(NULL, sb, AML_NAME('_', 'I', 'N', 'I'));
    if (sbIni != NULL)
    {
        DEREF_DEFER(sbIni);

        if (sbIni->type != AML_METHOD)
        {
            aml_state_deinit(&state);
            panic(NULL, "\\_SB_._INI is a '%s', not a method\n", aml_type_to_string(sbIni->type));
        }

        LOG_INFO("found \\_SB_._INI\n");
        if (aml_method_evaluate_integer(&state, sbIni, NULL) == ERR)
        {
            aml_state_deinit(&state);
            panic(NULL, "could not evaluate \\_SB_._INI\n");
        }
    }

    if (acpi_devices_init_children(&state, sb) == ERR)
    {
        aml_state_deinit(&state);
        panic(NULL, "could not initialize ACPI devices\n");
    }

    aml_state_deinit(&state);
}

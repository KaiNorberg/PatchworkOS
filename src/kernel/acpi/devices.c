#include <kernel/acpi/devices.h>

#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/aml/runtime/eisa_id.h>
#include <kernel/acpi/aml/runtime/method.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>

#include <errno.h>

static inline uint64_t acpi_hid_get(aml_state_t* state, aml_object_t* device, char* buffer)
{
    aml_object_t* hid = aml_namespace_find(&state->overlay, device, 1, AML_NAME('_', 'H', 'I', 'D'));
    if (hid == NULL)
    {
        return 1;
    }
    DEREF_DEFER(hid);

    if (hid->type == AML_INTEGER)
    {
        return aml_eisa_id_to_string(hid->integer.value, buffer);
    }
    else if (hid->type == AML_STRING)
    {
        strcpy(buffer, hid->string.content);
        return 0;
    }

    LOG_ERR("%s._HID is a '%s', not an Integer or String\n", AML_NAME_TO_STRING(device->name),
        aml_type_to_string(hid->type));
    errno = EILSEQ;
    return ERR;
}

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

static inline uint64_t acpi_device_init_children(aml_state_t* state, aml_object_t* device, bool shouldCallIni)
{
    aml_object_t* child;
    LIST_FOR_EACH(child, &device->children, siblingsEntry)
    {
        if (child->type != AML_DEVICE)
        {
            continue;
        }

        acpi_sta_flags_t sta;
        if (acpi_sta_get_flags(state, child, &sta) == ERR)
        {
            return ERR;
        }

        char hid[MAX_NAME];
        uint64_t hidResult = acpi_hid_get(state, child, hid);
        if (hidResult == ERR)
        {
            return ERR;
        }
        else if (hidResult == 0)
        {
            module_event_t event = {
                .type = MODULE_EVENT_LOAD,
                .load.hid = hid,
            };
            if (module_event(&event) == ERR)
            {
                LOG_WARN("could not load module for ACPI device '%s' with HID '%s'\n", AML_NAME_TO_STRING(child->name),
                    hid);
            }
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

        bool childShouldCallIni = shouldCallIni && (sta & (ACPI_STA_PRESENT | ACPI_STA_FUNCTIONAL));
        if (acpi_device_init_children(state, child, childShouldCallIni) == ERR)
        {
            return ERR;
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

    LOG_DEBUG("initializing ACPI devices under \\_SB_\n");
    if (acpi_device_init_children(&state, sb, true) == ERR)
    {
        aml_state_deinit(&state);
        panic(NULL, "could not initialize ACPI devices\n");
    }

    aml_state_deinit(&state);
}

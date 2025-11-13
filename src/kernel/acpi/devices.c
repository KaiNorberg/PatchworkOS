#include <_internal/MAX_NAME.h>
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

#include <errno.h>
#include <kernel/utils/ref.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    aml_state_t* state;
    char** hids;
    size_t hidCount;
} acpi_devices_init_ctx_t;

static uint64_t acpi_hid_push(acpi_devices_init_ctx_t* ctx, aml_object_t* device)
{
    aml_object_t* hid = aml_namespace_find(&ctx->state->overlay, device, 1, AML_NAME('_', 'H', 'I', 'D'));
    if (hid == NULL)
    {
        LOG_ERR("could not find %s._HID\n", AML_NAME_TO_STRING(device->name));
        return 0;
    }
    DEREF_DEFER(hid);

    aml_object_t* hidResult = aml_evaluate(ctx->state, hid, AML_STRING | AML_INTEGER);
    if (hidResult == NULL)
    {
        LOG_ERR("could not evaluate %s._HID\n", AML_NAME_TO_STRING(device->name));
        return ERR;
    }
    DEREF_DEFER(hidResult);

    ctx->hids = realloc(ctx->hids, sizeof(char*) * (ctx->hidCount + 1));
    if (ctx->hids == NULL)
    {
        DEREF(hidResult);
        return ERR;
    }

    if (hidResult->type == AML_STRING)
    {
        ctx->hids[ctx->hidCount] = malloc(hidResult->string.length + 1);
        if (ctx->hids[ctx->hidCount] == NULL)
        {
            DEREF(hidResult);
            return ERR;
        }
        memcpy(ctx->hids[ctx->hidCount], hidResult->string.content, hidResult->string.length);
        ctx->hids[ctx->hidCount][hidResult->string.length] = '\0';
        ctx->hidCount++;
    }
    else if (hidResult->type == AML_INTEGER)
    {
        char buffer[MAX_NAME];
        if (aml_eisa_id_to_string(hidResult->integer.value, buffer, sizeof(buffer)) == ERR)
        {
            LOG_ERR("%s._HID returned invalid EISA ID 0x%llx\n", AML_NAME_TO_STRING(device->name),
                hidResult->integer.value);
            errno = EILSEQ;
            return ERR;
        }

        ctx->hids[ctx->hidCount] = strdup(buffer);
        if (ctx->hids[ctx->hidCount] == NULL)
        {
            DEREF(hidResult);
            return ERR;
        }
        ctx->hidCount++;
    }
    else
    {
        LOG_ERR("%s._HID returned invalid type %s\n", AML_NAME_TO_STRING(device->name),
            aml_type_to_string(hidResult->type));
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

static uint64_t acpi_hid_push_str(acpi_devices_init_ctx_t* ctx, const char* hid)
{
    for (size_t i = 0; i < ctx->hidCount; i++)
    {
        if (strcmp(ctx->hids[i], hid) == 0)
        {
            return 0;
        }
    }

    ctx->hids = realloc(ctx->hids, sizeof(char*) * (ctx->hidCount + 1));
    if (ctx->hids == NULL)
    {
        return ERR;
    }

    ctx->hids[ctx->hidCount] = strdup(hid);
    if (ctx->hids[ctx->hidCount] == NULL)
    {
        return ERR;
    }
    ctx->hidCount++;

    return 0;
}

static uint64_t acpi_sta_get_flags(acpi_devices_init_ctx_t* ctx, aml_object_t* device, acpi_sta_flags_t* out)
{
    aml_object_t* sta = aml_namespace_find(&ctx->state->overlay, device, AML_NAME('_', 'S', 'T', 'A'));
    if (sta == NULL)
    {
        *out = ACPI_STA_FLAGS_DEFAULT;
        return 0;
    }
    DEREF_DEFER(sta);

    aml_object_t* staResult = aml_evaluate(ctx->state, sta, AML_INTEGER);
    if (staResult == NULL)
    {
        LOG_ERR("could not evaluate %s._STA\n", AML_NAME_TO_STRING(device->name));
        return ERR;
    }
    aml_integer_t value = staResult->integer.value;
    DEREF(staResult);

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

static aml_object_t* acpi_device_init_sb(acpi_devices_init_ctx_t* ctx)
{
    aml_object_t* sb = aml_namespace_find(NULL, NULL, 1, AML_NAME('_', 'S', 'B', '_'));
    if (sb == NULL) // Should never happen
    {
        LOG_ERR("could not find \\_SB_ in namespace\n");
        return NULL;
    }
    DEREF_DEFER(sb);

    acpi_sta_flags_t sta;
    if (acpi_sta_get_flags(ctx, sb, &sta) == ERR)
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
        DEREF_DEFER(ini);
        LOG_INFO("found \\_SB_._INI\n");
        aml_object_t* iniResult = aml_evaluate(ctx->state, ini, AML_ALL_TYPES);
        if (iniResult == NULL)
        {
            LOG_ERR("could not evaluate \\_SB_._INI\n");
            return NULL;
        }
        DEREF(iniResult);
    }

    return REF(sb);
}

static uint64_t acpi_device_init_children(acpi_devices_init_ctx_t* ctx, aml_object_t* device, bool shouldCallIni)
{
    aml_object_t* child = NULL;
    LIST_FOR_EACH(child, &device->children, siblingsEntry)
    {
        if (child->type != AML_DEVICE)
        {
            continue;
        }

        acpi_sta_flags_t sta;
        if (acpi_sta_get_flags(ctx, child, &sta) == ERR)
        {
            return ERR;
        }

        if (sta & ACPI_STA_PRESENT)
        {
            aml_object_t* ini = aml_namespace_find(NULL, child, AML_NAME('_', 'I', 'N', 'I'));
            if (ini != NULL)
            {
                DEREF_DEFER(ini);
                aml_object_t* iniResult = aml_evaluate(ctx->state, ini, AML_ALL_TYPES);
                if (iniResult == NULL)
                {
                    LOG_ERR("could not evaluate %s._INI\n", AML_NAME_TO_STRING(child->name));
                    return ERR;
                }
                DEREF(iniResult);
            }
        }

        if (acpi_hid_push(ctx, child) == ERR)
        {
            return ERR;
        }

        bool childShouldCallIni = shouldCallIni && (sta & (ACPI_STA_PRESENT | ACPI_STA_FUNCTIONAL));
        if (acpi_device_init_children(ctx, child, childShouldCallIni) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

static int acpi_hid_alphanum_cmp(const void* left, const void* right)
{
    // If alphabetic non-hex prefix differs, compare lexicographically
    const char* a = *(const char**)left;
    const char* b = *(const char**)right;
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

void acpi_devices_init(void)
{
    MUTEX_SCOPE(aml_big_mutex_get());

    aml_state_t state;
    if (aml_state_init(&state, NULL) == ERR)
    {
        panic(NULL, "could not initialize AML state for ACPI device initialization\n");
    }

    acpi_devices_init_ctx_t ctx = {
        .state = &state,
        .hids = NULL,
        .hidCount = 0,
    };

    aml_object_t* sb = acpi_device_init_sb(&ctx);
    if (sb == NULL)
    {
        aml_state_deinit(&state);
        panic(NULL, "could not initialize ACPI devices\n");
    }
    DEREF_DEFER(sb);

    LOG_DEBUG("initializing ACPI devices under \\_SB_\n");
    if (acpi_device_init_children(&ctx, sb, true) == ERR)
    {
        aml_state_deinit(&state);
        panic(NULL, "could not initialize ACPI devices\n");
    }

    // Because of... reasons some hardware wont report certain devices via ACPI
    // even if they actually do have it. In these cases we manually add their HIDs.

    if (acpi_tables_lookup("HPET", sizeof(sdt_header_t), 0) != NULL) // HPET
    {
        if (acpi_hid_push_str(&ctx, "PNP0103") == ERR)
        {
            aml_state_deinit(&state);
            panic(NULL, "could not initialize ACPI devices\n");
        }
    }

    if (acpi_tables_lookup("APIC", sizeof(sdt_header_t), 0) != NULL) // APIC
    {
        if (acpi_hid_push_str(&ctx, "PNP0003") == ERR)
        {
            aml_state_deinit(&state);
            panic(NULL, "could not initialize ACPI devices\n");
        }
    }

    qsort(ctx.hids, ctx.hidCount, sizeof(char*), acpi_hid_alphanum_cmp);
    for (size_t i = 0; i < ctx.hidCount; i++)
    {
        if (module_load(ctx.hids[i], MODULE_LOAD_ONE) == ERR)
        {
            panic(NULL, "Could not load module for ACPI device with HID '%s'\n", ctx.hids[i]);
        }
        free(ctx.hids[i]);
    }
    free(ctx.hids);

    aml_state_deinit(&state);
}

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
#include <kernel/acpi/tables.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct
{
    char hid[MAX_NAME];
    char cid[MAX_NAME];
    char path[MAX_PATH];
} acpi_device_id_t;

typedef struct
{
    acpi_device_id_t* ids;
    uint64_t idCount;
} acpi_devices_init_ctx_t;

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
        if (aml_eisa_id_to_string(idObject->integer.value, out, outSize) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        LOG_ERR("id object '%s' is of invalid type '%s'\n", AML_NAME_TO_STRING(idObject->name), aml_type_to_string(idObject->type));
        errno = EILSEQ;
        return ERR;
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
    DEREF_DEFER(sta);

    aml_object_t* staResult = aml_evaluate(NULL, sta, AML_INTEGER);
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


static uint64_t acpi_device_push(acpi_devices_init_ctx_t* ctx, const char* hid, const char* cid, const char* path)
{
    if (hid == NULL || hid[0] == '\0')
    {
        return 0;
    }

    acpi_device_id_t* newIds = realloc(ctx->ids, sizeof(acpi_device_id_t) * (ctx->idCount + 1));
    if (newIds == NULL)
    {
        return ERR;
    }

    ctx->ids = newIds;
    strncpy_s(ctx->ids[ctx->idCount].hid, MAX_NAME, hid, MAX_NAME);
    if (cid != NULL)
    {
        strncpy_s(ctx->ids[ctx->idCount].cid, MAX_NAME, cid, MAX_NAME);
    }
    else
    {
        ctx->ids[ctx->idCount].cid[0] = '\0';
    }
    strncpy_s(ctx->ids[ctx->idCount].path, MAX_PATH, path, MAX_PATH);
    ctx->idCount++;

    return 0;
}

static uint64_t acpi_device_push_if_not_exists(acpi_devices_init_ctx_t* ctx, const char* hid, const char* path)
{
    for (uint64_t i = 0; i < ctx->idCount; i++)
    {
        if (strcmp(ctx->ids[i].hid, hid) == 0)
        {
            return 0;
        }
    }

    return acpi_device_push(ctx, hid, NULL, path);
}

static uint64_t acpi_device_add(acpi_devices_init_ctx_t* ctx, aml_object_t* device, const char* path)
{
    acpi_device_id_t deviceId = {0};

    aml_object_t* hid = aml_namespace_find_child(NULL, device, AML_NAME('_', 'H', 'I', 'D'));
    if (hid == NULL)
    {
        return 0; // Nothing to do
    }
    DEREF_DEFER(hid);   

    aml_object_t* hidResult = aml_evaluate(NULL, hid, AML_STRING | AML_INTEGER);
    if (hidResult == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(hidResult);

    if (acpi_id_object_to_string(hidResult, deviceId.hid, MAX_NAME) == ERR)
    {
        return ERR;
    }    

    aml_object_t* cid = aml_namespace_find_child(NULL, device, AML_NAME('_', 'C', 'I', 'D'));
    if (cid != NULL)
    {
        DEREF_DEFER(cid);

        aml_object_t* cidResult = aml_evaluate(NULL, cid, AML_STRING | AML_INTEGER);
        if (cidResult == NULL)
        {
            return ERR;
        }
        DEREF_DEFER(cidResult);

        if (acpi_id_object_to_string(cidResult, deviceId.cid, MAX_NAME) == ERR)
        {
            return ERR;
        }
    }

    if (acpi_device_push(ctx, deviceId.hid, deviceId.cid, path) == ERR)
    {
        return ERR;
    }

    return 0;
}

static aml_object_t* acpi_device_init_sb(void)
{
    aml_object_t* sb = aml_namespace_find(NULL, NULL, 1, AML_NAME('_', 'S', 'B', '_'));
    if (sb == NULL) // Should never happen
    {
        LOG_ERR("could not find \\_SB_ in namespace\n");
        return NULL;
    }
    DEREF_DEFER(sb);

    acpi_sta_flags_t sta;
    if (acpi_sta_get_flags(sb, &sta) == ERR)
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
        aml_object_t* iniResult = aml_evaluate(NULL, ini, AML_ALL_TYPES);
        if (iniResult == NULL)
        {
            LOG_ERR("could not evaluate \\_SB_._INI\n");
            return NULL;
        }
        DEREF(iniResult);
    }

    return REF(sb);
}

static uint64_t acpi_device_init_children(acpi_devices_init_ctx_t* ctx, aml_object_t* device, const char* path, bool parentAllowsIni)
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
        if (acpi_sta_get_flags(child, &sta) == ERR)
        {
            return ERR;
        }

        bool isPresent = (sta & ACPI_STA_PRESENT);
        if (parentAllowsIni && isPresent)
        {
            aml_object_t* ini = aml_namespace_find_child(NULL, child, AML_NAME('_', 'I', 'N', 'I'));
            if (ini != NULL)
            {
                DEREF_DEFER(ini);
                aml_object_t* iniResult = aml_evaluate(NULL, ini, AML_ALL_TYPES);
                if (iniResult == NULL)
                {
                    LOG_ERR("could not evaluate %s._INI\n", childPath);
                    return ERR;
                }
                DEREF(iniResult);
            }
        }

        if (acpi_device_add(ctx, child, childPath) == ERR)
        {
            return ERR;
        }

        bool shouldEnumerateChildren = isPresent || (sta & ACPI_STA_FUNCTIONAL);
        if (shouldEnumerateChildren)
        {
            bool childrenShouldCallIni = parentAllowsIni && isPresent;
            if (acpi_device_init_children(ctx, child, childPath, childrenShouldCallIni) == ERR)
            {
                return ERR;
            }
        }
    }
    return 0;
}

static int acpi_device_id_compare(const void* left, const void* right)
{
    // If alphabetic non-hex prefix differs, compare lexicographically
    const char* a = (*(const acpi_device_id_t*)left).hid;
    const char* b = (*(const acpi_device_id_t*)right).hid;
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

    acpi_devices_init_ctx_t ctx = {
        .ids = NULL,
        .idCount = 0,
    };

    aml_object_t* sb = acpi_device_init_sb();
    if (sb == NULL)
    {
        panic(NULL, "Could not initialize ACPI devices\n");
    }
    DEREF_DEFER(sb);

    LOG_DEBUG("initializing ACPI devices under \\_SB_\n");
    if (acpi_device_init_children(&ctx, sb, "\\_SB_", true) == ERR)
    {
        panic(NULL, "Could not initialize ACPI devices\n");
    }

    // Because of... reasons some hardware wont report certain devices via ACPI
    // even if they actually do have it. In these cases we manually add their HIDs.

    if (acpi_tables_lookup("HPET", sizeof(sdt_header_t), 0) != NULL) // HPET
    {
        if (acpi_device_push_if_not_exists(&ctx, "PNP0103", "HPET") == ERR)
        {
            panic(NULL, "Could not initialize ACPI devices\n");
        }
    }

    if (acpi_tables_lookup("APIC", sizeof(sdt_header_t), 0) != NULL) // APIC
    {
        if (acpi_device_push_if_not_exists(&ctx, "PNP0003", "APIC") == ERR)
        {
            panic(NULL, "Could not initialize ACPI devices\n");
        }
    }

    if (ctx.ids != NULL && ctx.idCount > 0) 
    {
        qsort(ctx.ids, ctx.idCount, sizeof(acpi_device_id_t), acpi_device_id_compare);
    }

    for (size_t i = 0; i < ctx.idCount; i++)
    {
        uint64_t loadedModules = module_device_attach(ctx.ids[i].hid, ctx.ids[i].path, MODULE_LOAD_ONE);
        if (loadedModules == ERR)
        {
            LOG_ERR("could not load module for ACPI device with HID '%s'\n", ctx.ids[i].hid);
            continue;
        }

        if (loadedModules != 0 || ctx.ids[i].cid[0] == '\0')
        {
            continue;
        }

        loadedModules = module_device_attach(ctx.ids[i].cid, ctx.ids[i].path, MODULE_LOAD_ONE);
        if (loadedModules == ERR)
        {
            LOG_ERR("could not load module for ACPI device with CID '%s'\n", ctx.ids[i].cid);
        }
    }

    free(ctx.ids);
}
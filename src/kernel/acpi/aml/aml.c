#include "aml.h"

#include "acpi/tables.h"
#include "aml_patch_up.h"
#include "aml_predefined.h"
#include "aml_state.h"
#include "aml_integer.h"
#include "aml_to_string.h"
#include "encoding/term.h"
#include "log/log.h"
#include "sched/timer.h"

#ifdef DEBUG_TESTING
#include "aml_tests.h"
#endif

#include "log/log.h"

#include <errno.h>
#include <sys/math.h>

static mutex_t bigMutex;

static aml_object_t* root = NULL;

static inline uint64_t aml_init_parse_all(void)
{
    dsdt_t* dsdt = DSDT_GET();
    if (dsdt == NULL)
    {
        LOG_ERR("failed to retrieve DSDT\n");
        return ERR;
    }

    LOG_INFO("DSDT found containing %llu bytes of AML code\n", dsdt->header.length - sizeof(dsdt_t));

    const uint8_t* dsdtEnd = (const uint8_t*)dsdt + dsdt->header.length;
    if (aml_parse(dsdt->definitionBlock, dsdtEnd) == ERR)
    {
        LOG_ERR("failed to parse DSDT\n");
        return ERR;
    }

    uint64_t index = 0;
    ssdt_t* ssdt = NULL;
    while (true)
    {
        ssdt = SSDT_GET(index);
        if (ssdt == NULL)
        {
            break;
        }

        LOG_INFO("SSDT%llu found containing %llu bytes of AML code\n", index, ssdt->header.length - sizeof(ssdt_t));

        const uint8_t* ssdtEnd = (const uint8_t*)ssdt + ssdt->header.length;
        if (aml_parse(ssdt->definitionBlock, ssdtEnd) == ERR)
        {
            LOG_ERR("failed to parse SSDT%llu\n", index);
            return ERR;
        }

        index++;
    }

    LOG_INFO("parsed 1 DSDT and %llu SSDTs\n", index);

    return 0;
}

uint64_t aml_init(void)
{
    LOG_INFO("AML revision %d, init and parse all\n", ACPI_REVISION);

    mutex_init(&bigMutex);
    MUTEX_SCOPE(&bigMutex);

    if (aml_integer_handling_init() == ERR)
    {
        return ERR;
    }

    root = aml_object_new(NULL, AML_OBJECT_ROOT | AML_OBJECT_PREDEFINED);
    if (root == NULL)
    {
        return ERR;
    }

    if (aml_device_set(root) == ERR || aml_object_add(root, NULL, NULL) == ERR)
    {
        DEREF(root);
        root = NULL;
        return ERR;
    }

    if (aml_predefined_init() == ERR)
    {
        DEREF(root);
        root = NULL;
        return ERR;
    }

    if (aml_patch_up_init() == ERR)
    {
        DEREF(root);
        root = NULL;
        return ERR;
    }

#ifdef DEBUG_TESTING
    if (aml_tests_post_init() == ERR)
    {
        DEREF(root);
        root = NULL;
        return ERR;
    }
#endif

    if (aml_init_parse_all() == ERR)
    {
        DEREF(root);
        root = NULL;
        return ERR;
    }

    LOG_INFO("resolving %llu unresolved objects\n", aml_patch_up_unresolved_count());
    if (aml_patch_up_resolve_all() == ERR)
    {
        DEREF(root);
        root = NULL;
        LOG_ERR("failed to resolve unresolved object\n");
        return ERR;
    }

    if (aml_patch_up_unresolved_count() > 0)
    {
        DEREF(root);
        root = NULL;
        LOG_ERR("there are still %llu unresolved object\n", aml_patch_up_unresolved_count());
        return ERR;
    }

#ifdef DEBUG_TESTING
    if (aml_tests_post_parse_all() == ERR)
    {
        DEREF(root);
        root = NULL;
        return ERR;
    }
#endif

    return 0;
}

uint64_t aml_parse(const uint8_t* start, const uint8_t* end)
{
    if (start == NULL || end == NULL || start > end)
    {
        errno = EINVAL;
        return ERR;
    }

    if (start == end)
    {
        // Im not sure why but some firmwares have empty SSDTs.
        return 0;
    }

    // In section 20.2.1, we see the definition AMLCode := DefBlockHeader TermList.
    // The DefBlockHeader is already read as thats the `sdt_header_t`.
    // So the entire code is a termlist.

    aml_state_t state;
    if (aml_state_init(&state, start, end, 0, NULL, NULL) == ERR)
    {
        return ERR;
    }

    uint64_t result = aml_term_list_read(&state, aml_root_get(), end);

    if (aml_state_deinit(&state) == ERR)
    {
        LOG_ERR("failed to deinitialize AML state\n");
        return ERR;
    }
    return result;
}

aml_object_t* aml_root_get(void)
{
    if (root == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    return REF(root);
}

mutex_t* aml_big_mutex_get(void)
{
    return &bigMutex;
}

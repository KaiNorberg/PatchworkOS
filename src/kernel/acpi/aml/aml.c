#include "aml.h"

#include "acpi/tables.h"
#include "encoding/term.h"
#include "integer.h"
#include "log/log.h"
#include "namespace.h"
#include "patch_up.h"
#include "predefined.h"
#include "sched/timer.h"
#include "state.h"
#include "to_string.h"

#ifdef TESTING
#include "tests.h"
#endif

#include "log/log.h"

#include <errno.h>
#include <sys/math.h>

static mutex_t bigMutex;

static inline uint64_t aml_parse(const uint8_t* start, const uint8_t* end)
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
    if (aml_state_init(&state, NULL) == ERR)
    {
        return ERR;
    }

    aml_object_t* root = aml_namespace_get_root();
    DEREF_DEFER(root);

    uint64_t result = aml_term_list_read(&state, root, start, end, NULL);

    if (result != ERR)
    {
        aml_namespace_commit(&state.overlay);
    }

    aml_state_deinit(&state);
    return result;
}

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
    LOG_INFO("AML revision %d, init and parse all\n", AML_CURRENT_REVISION);

    mutex_init(&bigMutex);
    MUTEX_SCOPE(&bigMutex);

    aml_object_t* root = aml_object_new();
    if (root == NULL)
    {
        LOG_ERR("failed to create root object\n");
        return ERR;
    }
    DEREF_DEFER(root);

    // We dont need to add the root to the namespace map as it has no name.
    if (aml_predefined_scope_set(root) == ERR)
    {
        LOG_ERR("failed to set predefined scope on root object\n");

        return ERR;
    }

    if (aml_namespace_init(root) == ERR)
    {
        LOG_ERR("failed to init namespace\n");
        return ERR;
    }

    if (aml_integer_handling_init() == ERR)
    {
        LOG_ERR("failed to init integer handling\n");
        return ERR;
    }

    if (aml_predefined_init() == ERR)
    {
        LOG_ERR("failed to init predefined names\n");
        return ERR;
    }

    if (aml_patch_up_init() == ERR)
    {
        LOG_ERR("failed to init patch up\n");
        return ERR;
    }

#ifdef TESTING
    if (aml_tests_post_init() == ERR)
    {
        LOG_ERR("failed to run tests post init\n");
        return ERR;
    }
#endif

    if (aml_init_parse_all() == ERR)
    {
        LOG_ERR("failed to parse all tables\n");
        return ERR;
    }

    LOG_INFO("resolving %llu unresolved objects\n", aml_patch_up_unresolved_count());
    if (aml_patch_up_resolve_all() == ERR)
    {
        LOG_ERR("failed to resolve unresolved object\n");
        return ERR;
    }

    if (aml_patch_up_unresolved_count() > 0)
    {
        LOG_ERR("there are still %llu unresolved object\n", aml_patch_up_unresolved_count());
        return ERR;
    }

    // TODO: Reimplement sysfs exposure of AML namespace.
    /*if (aml_object_expose_in_sysfs(root) == ERR)
    {
        return ERR;
    }*/

#ifdef TESTING
    if (aml_tests_post_parse_all() == ERR)
    {
        LOG_ERR("failed to run tests post parse all\n");
        return ERR;
    }
#endif

    return 0;
}

mutex_t* aml_big_mutex_get(void)
{
    return &bigMutex;
}

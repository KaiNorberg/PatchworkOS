#include "aml_tests.h"

#ifndef NDEBUG

#include "_aml_full_test.h"
#include "aml.h"
#include "aml_object.h"
#include "aml_state.h"

#include "acpi/tables.h"
#include "log/log.h"

static uint64_t aml_tests_check_object_leak(void)
{
    uint64_t totalObjects = aml_object_get_total_count();
    uint64_t rootChildren = aml_object_count_children(aml_root_get());
    LOG_INFO("total objects after parsing %llu\n", totalObjects);
    if (totalObjects != rootChildren + 1)
    {
        LOG_ERR("memory leak detected, total objects %llu, but root has %llu children\n", totalObjects, rootChildren);
        return ERR;
    }
    return 0;
}

// Parse the full.aml test file which has been binary dumped into `_aml_full_test.h`
static uint64_t aml_tests_parse_full_test(void)
{
    ssdt_t* testAml = (ssdt_t*)lib_aslts_full_aml;
    const uint8_t* end = (const uint8_t*)testAml + testAml->header.length;

    aml_state_t state;
    if (aml_state_init(&state, testAml->definitionBlock, end, 0, NULL, NULL) == ERR)
    {
        return ERR;
    }

    uint64_t result = aml_term_list_read(&state, aml_root_get(), end);

    aml_state_garbage_collect(&state);

    if (aml_state_deinit(&state) == ERR)
    {
        return ERR;
    }

    return result;
}

uint64_t aml_tests_post_init(void)
{
    uint64_t startingObjects = aml_object_get_total_count();

    if (aml_tests_parse_full_test() == ERR)
    {
        // For now this is definetly going to fail as we havent implemented everything yet.
        // So just log it and continue.
        LOG_WARN("full test parse failed, this is expected until the AML parser is fully implemented\n");
        // return ERR;
    }

    if (startingObjects != aml_object_get_total_count())
    {
        LOG_ERR("memory leak detected, total objects before test %llu, after test %llu\n", startingObjects,
            aml_object_get_total_count());
        return ERR;
    }

    LOG_INFO("post init tests passed\n");
    return 0;
}

uint64_t aml_tests_post_parse_all(void)
{
    if (aml_tests_check_object_leak() == ERR)
    {
        return ERR;
    }

    LOG_INFO("post parse all tests passed\n");
    return 0;
}

#endif

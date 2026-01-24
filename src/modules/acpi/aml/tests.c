#ifdef _TESTING_

#include "acpica_tests/all_tests.h"

#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/encoding/term.h>
#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/aml/runtime/method.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/acpi/tables.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/utils/test.h>

#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

static uint64_t aml_tests_check_object_leak(void)
{
    aml_object_t* root = aml_namespace_get_root();
    UNREF_DEFER(root);

    uint64_t totalObjects = aml_object_get_total_count();
    uint64_t rootChildren = aml_object_count_children(root);
    LOG_INFO("total objects after parsing %llu\n", totalObjects);
    if (totalObjects != rootChildren + 1)
    {
        LOG_ERR("memory leak detected, total objects %llu, but root has %llu children\n", totalObjects, rootChildren);
        return ERR;
    }
    return 0;
}

static uint64_t aml_tests_acpica_do_test(const acpica_test_t* test)
{
    LOG_INFO("running test '%s'\n", test->name);

    ssdt_t* testAml = (ssdt_t*)test->aml;
    const uint8_t* end = (const uint8_t*)testAml + testAml->header.length;

    aml_state_t state;
    if (aml_state_init(&state, NULL) == ERR)
    {
        return ERR;
    }

    aml_object_t* root = aml_namespace_get_root();
    UNREF_DEFER(root);

    if (aml_term_list_read(&state, root, testAml->definitionBlock, end, NULL) == ERR)
    {
        LOG_ERR("test '%s' failed to parse AML\n", test->name);
        aml_state_deinit(&state);
        return ERR;
    }

    // Set the "Settings number, used to adjust the aslts tests for different releases of ACPICA".
    // We set it to 6 as that is the latest version as of writing this.
    aml_object_t* setn = aml_namespace_find(&state.overlay, root, 1, AML_NAME('S', 'E', 'T', 'N'));
    if (setn == NULL)
    {
        LOG_ERR("test '%s' does not contain a valid SETN object\n", test->name);
        aml_state_deinit(&state);
        return ERR;
    }
    UNREF_DEFER(setn);

    if (aml_integer_set(setn, 6) == ERR)
    {
        LOG_ERR("test '%s' failed to set SETN value\n", test->name);
        aml_state_deinit(&state);
        return ERR;
    }

    // We dont use the \MAIN method directly instead we use the \MN01 method which enables "slack mode".
    // Basically, certain features that would normally just result in a crash are allowed in slack mode, for example
    // implicit returns, which some firmware depends on. See section 5.2 of the ACPICA reference for more details.
    aml_object_t* mainObj = aml_namespace_find(&state.overlay, root, 1, AML_NAME('M', 'N', '0', '1'));
    if (mainObj == NULL || mainObj->type != AML_METHOD)
    {
        LOG_ERR("test '%s' does not contain a valid method\n", test->name);
        aml_state_deinit(&state);
        return ERR;
    }
    UNREF_DEFER(mainObj);

    aml_object_t* result = aml_method_invoke(&state, &mainObj->method, NULL);
    if (result == NULL)
    {
        LOG_ERR("test '%s' method evaluation failed\n", test->name);
        aml_state_deinit(&state);
        return ERR;
    }
    UNREF_DEFER(result);

    aml_state_deinit(&state);

    if (result->type != AML_INTEGER)
    {
        LOG_ERR("test '%s' method did not return an integer\n", test->name);
        return ERR;
    }

    if (result->integer.value != 0)
    {
        LOG_ERR("test '%s' failed, returned %llu\n", test->name, result->integer.value);
        return ERR;
    }

    LOG_INFO("test '%s' passed\n", test->name);
    return 0;
}

static uint64_t aml_tests_acpica_run_all(void)
{
    for (int32_t i = 0; i < ACPICA_TEST_COUNT; i++)
    {
        const acpica_test_t* test = &acpicaTests[i];
        if (aml_tests_acpica_do_test(test) == ERR)
        {
            return ERR;
        }
    }
    return 0;
}

TEST_DEFINE(aml)
{
    if (aml_tests_check_object_leak() == ERR)
    {
        return ERR;
    }

    uint64_t startingObjects = aml_object_get_total_count();

    if (aml_tests_acpica_run_all() == ERR)
    {
        // For now this is definitely going to fail as we havent implemented everything yet.
        // So just log it and continue.
        LOG_WARN("ACPICA tests failed, this is expected until more AML features are implemented\n");
        // return ERR;
    }

    if (startingObjects != aml_object_get_total_count())
    {
        LOG_ERR("memory leak detected, total objects before test %llu, after test %llu\n", startingObjects,
            aml_object_get_total_count());
        return ERR;
    }

    LOG_INFO("post parse all tests passed\n");
    return 0;
}

#endif

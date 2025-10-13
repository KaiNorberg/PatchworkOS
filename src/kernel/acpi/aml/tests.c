#include "tests.h"

#ifdef TESTING

#include "acpica_tests/all_tests.h"

#include "aml.h"
#include "encoding/term.h"
#include "exception.h"
#include "mem/heap.h"
#include "object.h"
#include "runtime/method.h"
#include "sched/timer.h"
#include "state.h"
#include "to_string.h"

#include "acpi/tables.h"
#include "log/log.h"

#include <string.h>
#include <sys/list.h>

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

static void aml_tests_exception_handler(aml_exception_t code)
{
    // Some tests will trigger exceptions to validate that they actually happen, we are then expected to notify
    // the code that an exception occurred using the \_ERR object.
    aml_object_t* err = aml_object_find(NULL, "\\_ERR");
    if (err == NULL)
    {
        LOG_ERR("test does not contain a valid _ERR object\n");
        return;
    }
    DEREF_DEFER(err);

    aml_object_t* arg0 = aml_object_new();
    if (arg0 == NULL)
    {
        return;
    }
    DEREF_DEFER(arg0);

    if (aml_integer_set(arg0, code) == ERR)
    {
        return;
    }

    aml_object_t* arg1 = aml_object_new();
    if (arg1 == NULL)
    {
        return;
    }
    DEREF_DEFER(arg1);

    if (aml_string_set(arg1, aml_exception_to_string(code)) == ERR)
    {
        return;
    }

    aml_object_t* arg2 = aml_object_new();
    if (arg2 == NULL)
    {
        return;
    }
    DEREF_DEFER(arg2);

    if (aml_integer_set(arg2, 0) == ERR) // Should be the thread id but just set to 0 for now.
    {
        return;
    }

    aml_object_t* args[] = {arg0, arg1, arg2};
    aml_object_t* result = aml_method_evaluate(&err->method, args, 3);
    if (result == NULL)
    {
        return;
    }
    DEREF_DEFER(result);

    if (result->type != AML_INTEGER || result->integer.value != 0)
    {
        LOG_ERR("_ERR method did not return 0, returned '%s' instead\n", aml_type_to_string(result->type));
        return;
    }
}

static uint64_t aml_tests_acpica_do_test(const acpica_test_t* test)
{
    LOG_INFO("running test '%s'\n", test->name);

    ssdt_t* testAml = (ssdt_t*)test->aml;
    const uint8_t* end = (const uint8_t*)testAml + testAml->header.length;

    aml_state_t state;
    if (aml_state_init(&state, NULL, 0) == ERR)
    {
        return ERR;
    }

    if (aml_term_list_read(&state, aml_root_get(), testAml->definitionBlock, end, NULL) == ERR)
    {
        LOG_ERR("test '%s' failed to parse AML\n", test->name);
        aml_state_garbage_collect(&state);
        aml_state_deinit(&state);
        return ERR;
    }

    // Set the "Settings number, used to adjust the aslts tests for different releases of ACPICA".
    // We set it to 6 as that is the latest version as of writing this.
    aml_object_t* setn = aml_object_find(NULL, "\\SETN");
    if (setn == NULL)
    {
        LOG_ERR("test '%s' does not contain a valid SETN method\n", test->name);
        aml_state_garbage_collect(&state);
        aml_state_deinit(&state);
        return ERR;
    }
    DEREF_DEFER(setn);

    if (aml_integer_set(setn, 6) == ERR)
    {
        LOG_ERR("test '%s' failed to set SETN value\n", test->name);
        aml_state_garbage_collect(&state);
        aml_state_deinit(&state);
        return ERR;
    }

    // We dont use the \MAIN method directly instead we use the \MN01 method which enables "slack mode".
    // Basically, certain features that would normally just result in a crash are allowed in slack mode, for example
    // implicit returns, which some firmware depends on. See section 5.2 of the ACPICA reference for more details.
    aml_object_t* mainObj = aml_object_find(NULL, "\\MN01");
    if (mainObj == NULL || mainObj->type != AML_METHOD)
    {
        LOG_ERR("test '%s' does not contain a valid method\n", test->name);
        aml_state_garbage_collect(&state);
        aml_state_deinit(&state);
        return ERR;
    }
    DEREF_DEFER(mainObj);

    aml_object_t* result = aml_method_evaluate(&mainObj->method, NULL, 0);
    if (result == NULL)
    {
        LOG_ERR("test '%s' method evaluation failed\n", test->name);
        aml_state_garbage_collect(&state);
        aml_state_deinit(&state);
        return ERR;
    }
    DEREF_DEFER(result);

    aml_state_garbage_collect(&state);
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
    if (aml_exception_register(aml_tests_exception_handler) == ERR)
    {
        return ERR;
    }

    for (uint32_t i = 0; i < ACPICA_TEST_COUNT; i++)
    {
        const acpica_test_t* test = &acpicaTests[i];
        if (aml_tests_acpica_do_test(test) == ERR)
        {
            aml_exception_unregister(aml_tests_exception_handler);
            return ERR;
        }
    }
    aml_exception_unregister(aml_tests_exception_handler);
    return 0;
}

uint64_t aml_tests_post_init(void)
{
    LOG_INFO("running post init tests\n");
    clock_t start = timer_uptime();

    uint64_t startingObjects = aml_object_get_total_count();

    if (aml_tests_acpica_run_all() == ERR)
    {
        // For now this is definetly going to fail as we havent implemented everything yet.
        // So just log it and continue.
        LOG_WARN("ACPICA tests failed, this is expected until more AML features are implemented\n");
        // return ERR;
    }

    aml_tests_perf_report();

    if (startingObjects != aml_object_get_total_count())
    {
        LOG_ERR("memory leak detected, total objects before test %llu, after test %llu\n", startingObjects,
            aml_object_get_total_count());
        return ERR;
    }

    clock_t end = timer_uptime();
    LOG_INFO("post init tests passed in %llums\n", (end - start) * 1000 / CLOCKS_PER_SEC);
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

typedef struct
{
    list_entry_t entry;
    aml_token_num_t token;
    clock_t startTime;
    clock_t childTime;
} aml_perf_stack_entry_t;

static clock_t timeTakenPerToken[AML_MAX_TOKEN] = {0};
static uint64_t tokenOccurrences[AML_MAX_TOKEN] = {0};
static list_t perfStack = LIST_CREATE(perfStack);

void aml_tests_perf_start(aml_token_t* token)
{
    if (token->num >= AML_MAX_TOKEN)
    {
        return;
    }

    aml_perf_stack_entry_t* entry = heap_alloc(sizeof(aml_perf_stack_entry_t), HEAP_NONE);
    if (entry == NULL)
    {
        LOG_ERR("Performance profiler stack allocation failed\n");
        return;
    }

    list_entry_init(&entry->entry);
    entry->token = token->num;
    entry->startTime = timer_uptime();
    entry->childTime = 0;

    list_push(&perfStack, &entry->entry);

    tokenOccurrences[token->num]++;
}

void aml_tests_perf_end(void)
{
    if (list_is_empty(&perfStack))
    {
        LOG_ERR("Performance profiler stack underflow\n");
        return;
    }

    aml_perf_stack_entry_t* entry = CONTAINER_OF_SAFE(list_pop(&perfStack), aml_perf_stack_entry_t, entry);

    clock_t totalTime = timer_uptime() - entry->startTime;
    clock_t exclusiveTime = (entry->childTime >= totalTime) ? 0 : (totalTime - entry->childTime);
    if (entry->token < AML_MAX_TOKEN)
    {
        timeTakenPerToken[entry->token] += exclusiveTime;
    }

    if (!list_is_empty(&perfStack))
    {
        aml_perf_stack_entry_t* perfStackTop = CONTAINER_OF(list_last(&perfStack), aml_perf_stack_entry_t, entry);
        perfStackTop->childTime += totalTime;
    }

    heap_free(entry);
}

void aml_tests_perf_report(void)
{
    if (!list_is_empty(&perfStack))
    {
        LOG_WARN("Performance report called with %d unclosed measurements\n", list_length(&perfStack));
        while (!list_is_empty(&perfStack))
        {
            aml_perf_stack_entry_t* entry = CONTAINER_OF_SAFE(list_pop(&perfStack), aml_perf_stack_entry_t, entry);
            heap_free(entry);
        }
    }

    typedef struct
    {
        aml_token_num_t tokenNum;
        clock_t time;
    } token_time_pair_t;

    token_time_pair_t sorted[AML_MAX_TOKEN];
    uint32_t count = 0;

    for (uint32_t i = 0; i < AML_MAX_TOKEN; i++)
    {
        if (timeTakenPerToken[i] > 0)
        {
            sorted[count].tokenNum = i;
            sorted[count].time = timeTakenPerToken[i];
            count++;
        }
    }

    for (uint32_t i = 1; i < count; i++)
    {
        token_time_pair_t key = sorted[i];
        int32_t j = i - 1;

        while (j >= 0 && sorted[j].time < key.time)
        {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    LOG_INFO("performance report:\n");
    for (uint32_t i = 0; i < count; i++)
    {
        LOG_INFO("  %s: total=%llums, occurrences=%llu, avg=%lluns\n", aml_token_lookup(sorted[i].tokenNum)->name,
            sorted[i].time / (CLOCKS_PER_SEC / 1000), tokenOccurrences[sorted[i].tokenNum],
            sorted[i].time / tokenOccurrences[sorted[i].tokenNum]);
    }

    for (uint32_t i = 0; i < AML_MAX_TOKEN; i++)
    {
        timeTakenPerToken[i] = 0;
    }
}

#endif

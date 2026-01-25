#pragma once

#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <time.h>

#ifndef _TESTING_
#error "This file is only meant to be used for testing"
#endif

/**
 * @brief Kernel Test Framework.
 * @defgroup kernel_utils_test Test
 * @ingroup kernel_utils
 *
 * @{
 */

/**
 * @brief Type of a test function.
 */
typedef uint64_t (*test_func_t)(void);

/**
 * @brief Structure representing a test case.
 * @struct test_t
 */
typedef struct test
{
    const char* name;
    test_func_t func;
} test_t;

/**
 * @brief Run all registered tests in the `._tests` section.
 */
#define TEST_ALL() \
    do \
    { \
        extern test_t _tests_start; \
        extern test_t _tests_end; \
        const test_t* test = &_tests_start; \
        while (test < &_tests_end) \
        { \
            LOG_INFO("running test '%s'\n", test->name); \
            clock_t start = clock_uptime(); \
            uint64_t result = test->func(); \
            clock_t end = clock_uptime(); \
            if (result == _FAIL) \
            { \
                LOG_ERR("test '%s' FAILED in %llu ms\n", test->name, (end - start) / (CLOCKS_PER_MS)); \
                panic(NULL, "test failure"); \
            } \
            else \
            { \
                LOG_INFO("test '%s' passed in %llu ms\n", test->name, (end - start) / (CLOCKS_PER_MS)); \
            } \
            test++; \
        } \
    } while (0)

/**
 * @brief Define a test function to be run by `TEST_ALL()`.
 *
 * This will register the test within the current module or if used in the kernel, the kernel itself.
 *
 * Any module that wants to use the testing framework must call `TEST_ALL()` on its own.
 *
 * @param name The name of the test function.
 */
#define TEST_DEFINE(_name) \
    uint64_t _test_func_##_name(void); \
    const test_t __test_##_name __attribute__((used, section("._tests"))) = { \
        .name = #_name, \
        .func = _test_func_##_name, \
    }; \
    uint64_t _test_func_##_name(void)

/**
 * @brief Assert a condition in a test.
 */
#define TEST_ASSERT(cond) \
    do \
    { \
        if (!(cond)) \
        { \
            LOG_ERR("TEST_ASSERT failed '%s' at %s:%d\n", #cond, __FILE__, __LINE__); \
            return _FAIL; \
        } \
    } while (0)

/** @} */
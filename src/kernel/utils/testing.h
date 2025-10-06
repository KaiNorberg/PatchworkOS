#pragma once
#ifndef NDEBUG

// TODO: Add more tests

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t (*test_func_t)(void);

typedef struct
{
    test_func_t func;
    const char* name;
} test_t;

extern test_t _testsStart[];
extern test_t _testsEnd[];

#define TESTING_REGISTER_TEST(function) \
    uint64_t function(void); \
    __attribute__((used, section(".tests"))) test_t tests_##function = {.func = function, .name = #function}; \
    uint64_t function(void)

void testing_run_tests(void);

#endif

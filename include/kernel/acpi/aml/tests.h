#pragma once

#include <stdint.h>

#ifdef TESTING

#include <kernel/acpi/aml/token.h>

/**
 * @brief Tests for the AML parser.
 * @defgroup kernel_acpi_aml_tests Tests
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Run all AML tests.
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_tests_run_all(void);

/** @} */

#endif

#pragma once

#include <stdint.h>

#ifndef NDEBUG

/**
 * @brief Tests for the AML parser.
 * @defgroup kernel_acpi_aml_tests Tests
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Tests to run after the parser is initialized but before any AML is parsed.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_tests_post_init(void);

/**
 * @brief Tests to run after all AML is parsed.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_tests_post_parse_all(void);

/** @} */

#endif

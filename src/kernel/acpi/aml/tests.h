#pragma once

#include <stdint.h>

#ifdef TESTING

#include "token.h"

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
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_tests_post_init(void);

/**
 * @brief Tests to run after all AML is parsed.
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_tests_post_parse_all(void);

/**
 * @brief Start performance measurement for a specific AML token.
 *
 * Will be called in `aml_term_obj_read()` before starting to parse the token.
 *
 * @param token The token to start measuring performance for.
 */
void aml_tests_perf_start(aml_token_t* token);

/**
 * @brief End performance measurement for a specific AML token.
 *
 * Will be called in `aml_term_obj_read()` after finishing parsing the token.
 */
void aml_tests_perf_end(void);

/**
 * @brief Report the collected performance measurements.
 */
void aml_tests_perf_report(void);

/** @} */

#endif

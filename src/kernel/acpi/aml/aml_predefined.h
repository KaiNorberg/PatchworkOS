#pragma once

#include <stdint.h>

/**
 * @brief Predefined AML names and objects
 * @defgroup kernel_acpi_aml_predefined Predefined
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Initialize predefined AML names and objects.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_predefined_init(void);

/** @} */

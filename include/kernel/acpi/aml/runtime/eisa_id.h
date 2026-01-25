#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief EISA ID to string and vice versa conversion
 * @defgroup kernel_acpi_aml_runtime_eisa_id EISA ID
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Convert a string EISA ID to a numeric EISA ID.
 *
 * The conversion rules for EISA were derived from section 19.3.4 ASL Macros of the ACPI specification.
 *
 * @param str Pointer to the string EISA ID. Must be 7 characters long.
 * @return On success, the numeric EISA ID. On failure, `_FAIL` and `errno` is set.
 */
uint64_t aml_eisa_id_from_string(const char* str);

/**
 * @brief Convert a numeric EISA ID to a string EISA ID.
 *
 * The conversion rules for EISA were derived from section 19.3.4 ASL Macros of the ACPI specification.
 *
 * @param eisaId The numeric EISA ID.
 * @param buffer Pointer to a buffer to write the string EISA ID to. Must be at least 8 bytes long.
 * @param bufferSize The size of the buffer in bytes.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t aml_eisa_id_to_string(uint32_t eisaId, char* buffer, size_t bufferSize);

/** @} */

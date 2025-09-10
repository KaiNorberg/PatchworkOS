#pragma once

#include "acpi/aml/aml_state.h"

#include <stdint.h>

/**
 * @brief ACPI AML Data Objects Encoding
 * @defgroup kernel_acpi_aml_data Data Objects
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.3 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief ACPI AML ByteData structure.
 */
typedef uint8_t aml_byte_data_t;

/**
 * @brief Read a ByteData structure from the AML stream.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the data will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_byte_data_read(aml_state_t* state, aml_byte_data_t* out);

/** @} */

#pragma once

#include <kernel/acpi/aml/aml.h>

/**
 * @brief Extract Portion of Buffer or String
 * @defgroup kernel_acpi_aml_mid Mid
 * @ingroup kernel_acpi_aml
 *
 * @see Section 19.6.86 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Performs a "mid" operation, extracting a portion of a buffer or string.
 *
 * @param state The AML state.
 * @param bufferString The buffer or string object to extract from.
 * @param index The starting index for the extraction.
 * @param length The length of the portion to extract.
 * @return On success, the new buffer or string object containing the extracted portion. On failure, `NULL` and `errno`
 * is set.
 */
aml_object_t* aml_mid(aml_state_t* state, aml_object_t* bufferString, aml_uint_t index, aml_uint_t length);

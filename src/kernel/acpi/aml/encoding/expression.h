#pragma once

#include "acpi/aml/aml_state.h"
#include "data_types.h"

/**
 * @brief ACPI AML Expression Opcodes Encoding
 * @defgroup kernel_acpi_aml_expression Expression Opcodes
 * @ingroup kernel_acpi_aml
 * *
 * See section 20.2.5.4 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief ACPI AML BufferSize structure.
 */
typedef aml_qword_data_t aml_buffer_size_t;

/**
 * @brief Reads a BufferSize structure from the AML byte stream.
 *
 * A BufferSize structure is defined as `BufferSize := TermArg => Integer`.
 *
 * See section 19.6.10 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the buffer size will be stored.
 * @return uint64_t On success, the buffer size. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_size_read(aml_state_t* state, aml_buffer_size_t* out);

/**
 * @brief Reads a DefBuffer structure from the AML byte stream.
 *
 * The DefBuffer structure is defined as `DefBuffer := BufferOp PkgLength BufferSize ByteList`.
 *
 * See section 19.6.10 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the pointer to the ByteList will be stored.
 * @param outLength Pointer to the buffer where the length of the buffer will be stored.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_buffer_read(aml_state_t* state, uint8_t** out, uint64_t* outLength);

/** @} */

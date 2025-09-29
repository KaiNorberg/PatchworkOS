#pragma once

#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_state.h"

/**
 * @brief Arg Objecs Encoding
 * @defgroup kernel_acpi_aml_args Args
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.6.1 of the ACPI specification.
 *
 * @{
 */

/**
 * @brief Maximum number of arguments that can be passed to a method.
 */
#define AML_MAX_ARGS 7

/**
 * @brief Reads a ArgObj structure from the AML byte stream.
 *
 * A ArgObj is defined as `ArgObj := Arg0Op | Arg1Op | Arg2Op | Arg3Op | Arg4Op | Arg5Op | Arg6Op` where
 * - Arg0Op := 0x68,
 * - Arg1Op := 0x69,
 * - Arg2Op := 0x6A,
 * - Arg3Op := 0x6B,
 * - Arg4Op := 0x6C,
 * - Arg5Op := 0x6D and
 * - Arg6Op := 0x6E.
 *
 * @param state Pointer to the AML state.
 * @param out Pointer to the pointer to store the resulting Arg node.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_arg_obj_read(aml_state_t* state, aml_node_t** out);

/** @} */

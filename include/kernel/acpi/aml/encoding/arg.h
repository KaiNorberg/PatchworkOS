#pragma once

typedef struct aml_object aml_object_t;
typedef struct aml_term_list_ctx aml_term_list_ctx_t;

/**
 * @brief Arg Objecs Encoding
 * @defgroup kernel_acpi_aml_encoding_args Args
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
 * Note that if an ArgObj is storing a ObjectRefernce it will be Automatically dereferenced, so in such a situation
 * `out` will point to the actual object and not an ObjectReference in the ArgObj.
 *
 * @see Section 19.3.5.8.1 of the ACPI specification for more details.
 *
 * @param state Pointer to the AML state.
 * @return On success, the ArbObj. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_arg_obj_read(aml_term_list_ctx_t* ctx);

/** @} */

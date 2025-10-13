#pragma once

#include "acpi/aml/object.h"

#include <stdint.h>

typedef struct aml_state aml_state_t;

/**
 * @brief Term Objects Encoding
 * @defgroup kernel_acpi_aml_encoding_term Term Objects
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.5 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Stop reason.
 * @enum aml_stop_reason_t
 */
typedef enum
{
    AML_STOP_REASON_NONE,    ///< No stop reason, continue execution or has reached the end of the TermList
    AML_STOP_REASON_RETURN,  ///< A Return statement was hit
    AML_STOP_REASON_BREAK,   ///< A Break statement was hit
    AML_STOP_REASON_CONTINUE ///< A Continue statement was hit
} aml_stop_reason_t;

/**
 * @brief Context for reading a TermList.
 *
 * This structure is used to keep track of the state while reading a TermList from the AML byte stream.
 */
typedef struct aml_term_list_ctx
{
    aml_state_t* state;
    aml_object_t* scope;
    const uint8_t* start;
    const uint8_t* end;
    const uint8_t* current;
    aml_stop_reason_t stopReason;
} aml_term_list_ctx_t;

/**
 * @brief Reads an TermArg structure from the AML byte stream.
 *
 * A TermArg is defined as `TermArg := ExpressionOpcode | DataObject | ArgObj | LocalObj`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param allowedTypes Bitmask of allowed types for the TermArg, the  will be evaluated to one of these types.
 * @return On success, the TermArg object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_term_arg_read(aml_term_list_ctx_t* ctx, aml_type_t allowedTypes);

/**
 * @brief Wrapper around `aml_term_arg_read()` that converts the result to an integer.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out The output buffer to store the integer value of the TermArg.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_read_integer(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Wrapper around `aml_term_arg_read()` that converts the result to a string.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, the string. On failure, `NULL` and `errno` is set.
 */
aml_string_obj_t* aml_term_arg_read_string(aml_term_list_ctx_t* ctx);

/**
 * @brief Wrapper around `aml_term_arg_read()` that converts the result to a buffer.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, the buffer. On failure, `NULL` and `errno` is set.
 */
aml_buffer_obj_t* aml_term_arg_read_buffer(aml_term_list_ctx_t* ctx);

/**
 * @brief Wrapper around `aml_term_arg_read()` that converts the result to a package.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, the package. On failure, `NULL` and `errno` is set.
 */
aml_package_obj_t* aml_term_arg_read_package(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads an Object structure from the AML byte stream.
 *
 * An Object is defined as `Object := NameSpaceModifierObj | NamedObj`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a TermObj structure from the AML byte stream.
 *
 * A TermObj is defined as `TermObj := Object | StatementOpcode | ExpressionOpcode`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_obj_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a TermList structure from the AML byte stream.
 *
 * A TermList structure is defined as `TermList := Nothing | <termobj termlist>`.
 *
 * This is the biggest "structure" in AML, and the entry point for AML execution.
 *
 * Will not advance the parent TermLists current pointer.
 *
 * @param state The AML state.
 * @param scope The location in the namespace from which names will be resolved.
 * @param start Pointer to the start of the TermList in the AML byte stream.
 * @param end Pointer to the end of the TermList in the AML byte stream.
 * @param parentCtx The previous TermList context, or `NULL` if this is the top-level TermList, used to propagate stop
 * reasons.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_list_read(aml_state_t* state, aml_object_t* scope, const uint8_t* start, const uint8_t* end,
    aml_term_list_ctx_t* parentCtx);

/** @} */

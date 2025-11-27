#pragma once

#include <modules/acpi/aml/encoding/term.h>
#include <modules/acpi/aml/token.h>

/**
 * @brief Debugging
 * @defgroup modules_acpi_aml_debug Debugging
 * @ingroup modules_acpi_aml
 *
 * @{
 */

/**
 * @brief Log a debug error message with context information.
 *
 * Errors should be used for unrecoverable faults such as invalid AML bytecode or runtime errors like runing out of
 * memory.
 *
 * @param ctx The AML term list context.
 * @param function The function name where the error occurred.
 * @param format The format string for the error message.
 * @param ... Additional arguments for the format string.
 */
void aml_debug_error(aml_term_list_ctx_t* ctx, const char* function, const char* format, ...);

/**
 * @brief Macro to simplify calling `aml_debug_error()` with the current function name.
 */
#define AML_DEBUG_ERROR(ctx, format, ...) aml_debug_error(ctx, __func__, format, ##__VA_ARGS__)

/** @} */

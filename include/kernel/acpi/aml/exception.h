#pragma once

#include <kernel/acpi/aml/state.h>

#include <errno.h>
#include <stdint.h>

/**
 * @brief Exception handling
 * @defgroup kernel_acpi_aml_exception Exceptions
 * @ingroup kernel_acpi_aml
 *
 * When a non-fatal error occurs during AML execution an exception is raised, this module implements the exception
 * handling mechanism.
 *
 * @{
 */

/**
 * @brief AML exception codes
 * @enum aml_exception_t
 *
 * These values are taken from ACPICA (lib/acpica/tests/aslts/src/runtime/cntl/ehandle.asl) for compatibility with their
 * runtime test suite. Otherwise they could be any values.
 *
 * TODO: Implement handling and checking for all of these.
 */
typedef enum
{
    AML_NOT_FOUND = 0x5,
    AML_ERROR = 0x3001,
    AML_PARSE = 0x3002,
    AML_DIVIDE_BY_ZERO = 0x300E,
} aml_exception_t;

/**
 * @brief AML exception handler function type.
 * @typedef aml_exception_handler_t
 */
typedef void (*aml_exception_handler_t)(aml_state_t* state, aml_exception_t code);

/**
 * @brief Converts an AML exception code to a string.
 *
 * @param code The exception code to convert.
 * @return The string representation of the exception code, or "AE_AML_UNKNOWN_EXCEPTION" if the code is not recognized.
 */
const char* aml_exception_to_string(aml_exception_t code);

/**
 * @brief Registers an AML exception handler.
 *
 * The handler will be called whenever an AML exception is raised.
 *
 * Does not allow duplicates.
 *
 * @param handler The handler to register.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_exception_register(aml_exception_handler_t handler);

/**
 * @brief Unregisters an AML exception handler.
 *
 * @param handler The handler to unregister.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
void aml_exception_unregister(aml_exception_handler_t handler);

/**
 * @brief Raises an AML exception.
 *
 * @param state The AML state in which the exception occurred.
 * @param code The exception code to raise.
 * @param function The name of the function raising the exception, used for logging.
 */
void aml_exception_raise(aml_state_t* state, aml_exception_t code, const char* function);

/**
 * @brief Macro to raise an AML exception with the current function name.
 *
 * @param state The AML state in which the exception occurred.
 * @param code The exception code to raise.
 */
#define AML_EXCEPTION_RAISE(state, code) aml_exception_raise(state, code, __func__)

/** @} */

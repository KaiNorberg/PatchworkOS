#pragma once

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
    AML_ERROR = 0x3001,
    AML_PARSE = 0x3002,
    AML_BAD_OPCODE = 0x3003,
    AML_NO_OPERAND = 0x3004,
    AML_OPERAND_TYPE = 0x3005,
    AML_OPERAND_VALUE = 0x3006,
    AML_UNINITIALIZED_LOCAL = 0x3007,
    AML_UNINITIALIZED_ARG = 0x3008,
    AML_UNINITIALIZED_ELEMENT = 0x3009,
    AML_NUMERIC_OVERFLOW = 0x300A,
    AML_REGION_LIMIT = 0x300B,
    AML_BUFFER_LIMIT = 0x300C,
    AML_PACKAGE_LIMIT = 0x300D,
    AML_DIVIDE_BY_ZERO = 0x300E,
    AML_BAD_NAME = 0x300F,
    AML_NAME_NOT_FOUND = 0x3010,
    AML_INTERNAL = 0x3011,
    AML_INVALID_SPACE_ID = 0x3012,
    AML_STRING_LIMIT = 0x3013,
    AML_NO_RETURN_VALUE = 0x3014,
    AML_METHOD_LIMIT = 0x3015,
    AML_NOT_OWNER = 0x3016,
    AML_MUTEX_ORDER = 0x3017,
    AML_MUTEX_NOT_ACQUIRED = 0x3018,
    AML_INVALID_RESOURCE_TYPE = 0x3019,
    AML_INVALID_INDEX = 0x301A,
    AML_REGISTER_LIMIT = 0x301B,
    AML_NO_WHILE = 0x301C,
    AML_ALIGNMENT = 0x301D,
    AML_NO_RESOURCE_END_TAG = 0x301E,
    AML_BAD_RESOURCE_VALUE = 0x301F,
    AML_CIRCULAR_REFERENCE = 0x3020,
} aml_exception_t;

/**
 * @brief AML exception handler function type.
 * @typedef aml_exception_handler_t
 */
typedef void (*aml_exception_handler_t)(aml_exception_t code);

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
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_exception_register(aml_exception_handler_t handler);

/**
 * @brief Unregisters an AML exception handler.
 *
 * @param handler The handler to unregister.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
void aml_exception_unregister(aml_exception_handler_t handler);

/**
 * @brief Raises an AML exception.
 *
 * @param code The exception code to raise.
 * @param function The name of the function raising the exception, used for logging.
 */
void aml_exception_raise(aml_exception_t code, const char* function);

/**
 * @brief Macro to raise an AML exception with the current function name.
 *
 * @param code The exception code to raise.
 */
#define AML_EXCEPTION_RAISE(code) aml_exception_raise(code, __func__)

/** @} */

#pragma once

#include "acpi/aml/aml_object.h"

#include <stdint.h>

/**
 * @brief Data Type Conversion
 * @defgroup kernel_acpi_aml_convert Convert
 * @ingroup kernel_acpi_aml
 *
 * @see Section 19.3.5 of the ACPI specification for more details.
 * @see Section 19.3.5.7 table 19.6 for the conversion priority order.
 * @see Section 19.3.5.7 table 19.7 for a summary of the conversion rules.
 *
 * @{
 */

/**
 * @brief Converts the data in the source object to a allowed type and stores it in the destination object.
 *
 * Follows the rules in table 19.6 section 19.3.5.6 of the ACPI specification.
 *
 * See Section 19.3.5.6 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to convert.
 * @param dest Pointer to the destination object where the converted value will be stored, can be of type
 * `AML_UNINITIALIZED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert(aml_object_t* src, aml_object_t* dest, aml_type_t allowedTypes);

/**
 * @brief Performs a "Implicit Source Operand Conversion" acording to the rules in section 19.3.5.4 of the ACPI
 * specification.
 *
 * If `dest` is `NULL` then either, a new object is allocated and assigned to `*dest`, or `*dest` will be set to a
 * reference `src` if no conversion is needed.
 *
 * If `dest` is not `NULL` then the object pointed to by `*dest` will be set to the converted value or a copy of `src`
 * if no conversion is needed.
 *
 * This `dest` handling is to allow for the common case where the source object does not need to be converted. In which
 * case we can avoid a allocation and a copy buts its also just a requirement. For instance if we are implementing Index
 * and the source in a buffer well then we need the created BufferField to point to the original buffer not a copy of
 * it.
 *
 * @see Section 19.3.5.4 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to convert, if `AML_ARG` or `AML_LOCAL`, the value object will be used.
 * @param dest Pointer to the object pointer where the converted value will be stored, see above for details.
 * @param allowedTypes Bitmask of allowed destination types.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_source(aml_object_t* src, aml_object_t** dest, aml_type_t allowedTypes);

/**
 * @brief Performs a "Implicit Result Object Conversion" acording to the rules in section 19.3.5.5 of the ACPI
 * specification.
 *
 * @see Section 19.3.5.5 of the ACPI specification for more details.
 *
 * @param result Pointer to the result object to convert.
 * @param target Pointer to the target object to store the result in. For convenience this can be `NULL`, in which case
 * this does nothing.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_result(aml_object_t* result, aml_object_t* target);

/**
 * @brief Converts an integer to its Binary-Coded Decimal (BCD) representation.
 *
 * Binary-Coded decimal (BCD) is a format where instead of each bit representing a power of two, the Integer is split
 * into its individual decimal digits, and each digit is represented by a fixed number of bits. For example, the integer
 * `45` would be represented in BCD as `0x45` or in binary `0100 0101`.
 *
 * The number of bits per digit varies and the ACPI specification does not seem to specify how many should be
 * used, nor really anything at all about BCD. However, the most common representation seems to be 4 bits per digit,
 * which is what this function uses. The spec also does not specify what to do if the integer is too large to fit
 * in the BCD representation, so we just ignore it. I love ACPI.
 *
 * @param value The integer value to convert.
 * @param out Pointer to the output buffer where the BCD representation will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_integer_to_bcd(aml_integer_t value, aml_integer_t* out);

/**
 * @brief Converts a Integer, String or Buffer source object to a Buffer destination object.
 *
 * Note that this behaviour is different from the implicit source operand conversion and implicit result object
 * conversion rules.
 *
 * @see Section 19.6.138 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to convert. Must be of type Integer, String or Buffer.
 * @param dest Pointer to the destination object where the converted Buffer will be stored, can be of type
 * `AML_UNINITIALIZED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_to_buffer(aml_object_t* src, aml_object_t* dest);

/**
 * @brief Converts a Integer, String or Buffer source object to a String destination object in decimal format.
 *
 * Note that this behaviour is different from the implicit source operand conversion and implicit result object
 * conversion rules.
 *
 * @see Section 19.6.139 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to convert. Must be of type Integer, String or Buffer.
 * @param dest Pointer to the destination object where the converted Decimal String will be stored, can be of type
 * `AML_UNINITIALIZED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_to_decimal_string(aml_object_t* src, aml_object_t* dest);

/**
 * @brief Converts a Integer, String or Buffer source object to a String destination object in hexadecimal format.
 *
 * Note that this behaviour is different from the implicit source operand conversion and implicit result object
 * conversion rules.
 *
 * @see Section 19.6.140 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to convert. Must be of type Integer, String or Buffer.
 * @param dest Pointer to the destination object where the converted Hex String will be stored, can be of type
 * `AML_UNINITIALIZED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_to_hex_string(aml_object_t* src, aml_object_t* dest);

/**
 * @brief Converts a Integer, String or Buffer source object to an Integer destination object.
 *
 * Note that this behaviour is different from the implicit source operand conversion and implicit result object
 * conversion rules.
 *
 * @see Section 19.6.141 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to convert. Must be of type Integer, String or Buffer.
 * @param dest Pointer to the destination object where the converted Integer will be stored, can be of type
 * `AML_UNINITIALIZED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_to_integer(aml_object_t* src, aml_object_t* dest);

/** @} */

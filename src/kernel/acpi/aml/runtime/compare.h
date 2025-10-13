#pragma once

#include "acpi/aml/object.h"

/**
 * @brief Object Comparison
 * @defgroup kernel_acpi_aml_compare Compare
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Types of comparisons that can be performed between two ACPI objects.
 * @enum aml_object_compare_type_t
 */
typedef enum
{
    AML_COMPARE_AND = 0,     ///< Section 19.6.69, integer only
    AML_COMPARE_EQUAL = 1,   ///< Section 19.6.70
    AML_COMPARE_GREATER = 2, ///< Section 19.6.71
    AML_COMPARE_LESS = 3,    ///< Section 19.6.73
    AML_COMPARE_OR = 4,      ///< Section 19.6.80, integer only

    AML_COMPARE_INVERT_BASE = 0xFF, ///< All operations above this value are inverted versions of the base operations.
    AML_COMPARE_NOT_EQUAL = AML_COMPARE_INVERT_BASE + AML_COMPARE_EQUAL,
    AML_COMPARE_LESS_EQUAL = AML_COMPARE_INVERT_BASE + AML_COMPARE_GREATER,
    AML_COMPARE_GREATER_EQUAL = AML_COMPARE_INVERT_BASE + AML_COMPARE_LESS,
} aml_compare_operation_t;

/**
 * @brief Perform a logical NOT operation on an integer value.
 *
 * @param value The integer value to negate.
 * @return `AML_TRUE` if the input value is `AML_FALSE`, otherwise `AML_FALSE`.
 */
aml_integer_t aml_compare_not(aml_integer_t value);

/**
 * @brief Compare two ACPI objects.
 *
 * Only objects of type `AML_OBJECT_INTEGER`, `AML_OBJECT_STRING` and `AML_OBJECT_BUFFER` can be compared, and certain
 * operations only support `AML_OBJECT_INTEGER`. Both `a` and `b` must be of the same type, otherwise return false.
 *
 * @param a Pointer to the first object.
 * @param b Pointer to the second object.
 * @param operation The comparison operation to perform.
 * @return `AML_TRUE` if the comparison is true, `AML_FALSE` if the comparison is false.
 */
aml_integer_t aml_compare(aml_object_t* a, aml_object_t* b, aml_compare_operation_t operation);

/** @} */

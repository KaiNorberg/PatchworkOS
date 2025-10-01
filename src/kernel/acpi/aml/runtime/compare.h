#pragma once

#include "acpi/aml/aml_node.h"

/**
 * @brief Node Comparison
 * @defgroup kernel_acpi_aml_compare Compare
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Types of comparisons that can be performed between two ACPI nodes.
 * @enum aml_node_compare_type_t
 */
typedef enum
{
    AML_COMPARE_AND = 0, //!< Section 19.6.69, integer only
    AML_COMPARE_EQUAL = 1, //!< Section 19.6.70
    AML_COMPARE_GREATER = 2, //!< Section 19.6.71
    AML_COMPARE_LESS = 3, //!< Section 19.6.73
    AML_COMPARE_NOT = 4, //!< Section 19.6.75, integer only
    AML_COMPARE_OR = 5, //!< Section 19.6.80, integer only

    AML_COMPARE_INVERT_BASE = 0xFF, //!< All operations above this value are inverted versions of the base operations.
    AML_COMPARE_NOT_EQUAL = AML_COMPARE_INVERT_BASE + AML_COMPARE_EQUAL,
    AML_COMPARE_LESS_EQUAL = AML_COMPARE_INVERT_BASE + AML_COMPARE_GREATER,
    AML_COMPARE_GREATER_EQUAL = AML_COMPARE_INVERT_BASE + AML_COMPARE_LESS,
} aml_compare_operation_t;

/**
 * @brief Compare two ACPI nodes.
 *
 * Only nodes of type `AML_NODE_INTEGER`, `AML_NODE_STRING` and `AML_NODE_BUFFER` can be compared, and certain operations only
 * support `AML_NODE_INTEGER`.
 *
 * @param a Pointer to the first node.
 * @param b Pointer to the second node.
 * @param operation The comparison operation to perform.
 * @return true if the comparison is true, false otherwise or on error.
 */
bool aml_compare(aml_node_t* a, aml_node_t* b, aml_compare_operation_t operation);

/** @} */

#pragma once

#include "name.h"

typedef struct aml_data_object aml_data_object_t;

/**
 * @brief ACPI AML ObjectReference structure
 * @defgroup kernel_acpi_aml_object_reference Object Reference
 * @ingroup kernel_acpi_aml
 *
 * I am unable to find any proper definition of the ObjectReference structure in the ACPI specification. All the
 * mentions of it are circular as in "Object Reference | Reference to an object INITd using the RefOf, Index or
 * CondRefOf operators" (section 19.3.5), thanks guys that tells me so much.
 *
 * From what I can gather, it is simply a pointer to any "Object" (a term that's used multiple times and each time it
 * means something else), so we represent it as a `aml_node_t` pointer. In practice this is almost certainly the correct
 * interpretation but its quite frustrating that the specification is so vague about it.
 *
 * @{
 */

/**
 * @brief ACPI AML ObjectReference structure.
 * @struct aml_object_reference_t
 */
typedef struct aml_object_reference
{
    aml_node_t* node; //!< Pointer to the node in the ACPI namespace.
} aml_object_reference_t;

/**
 * @brief Initialize an ObjectReference structure.
 *
 * @param ref Pointer to the ObjectReference structure to initialize.
 * @param node Pointer to the node in the ACPI namespace, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_reference_init(aml_object_reference_t* ref, aml_node_t* node);

/**
 * @brief Deinitialize an ObjectReference structure.
 *
 * @param ref Pointer to the ObjectReference structure to deinitialize.
 */
void aml_object_reference_deinit(aml_object_reference_t* ref);

/**
 * @brief Check if an ObjectReference is null.
 *
 * @param ref Pointer to the ObjectReference structure to check.
 * @return true if the ObjectReference is null, false otherwise.
 */
bool aml_object_reference_is_null(aml_object_reference_t* ref);

/**
 * @brief Dereference an ObjectReference to get the underlying node.
 *
 * @param ref Pointer to the ObjectReference structure to dereference.
 * @return On success, a pointer to the underlying node. If the reference is null, `NULL` is returned.
 */
aml_node_t* aml_object_reference_deref(aml_object_reference_t* ref);

/** @} */

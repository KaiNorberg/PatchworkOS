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
 * means something else), so we represent it as both a `aml_data_object_t` pointer and a `aml_node_t` pointer that way
 * we can "Reference" both nodes in the namespace and DataObjects. In practice this is almost certainly the correct
 * interpretation but its quite frustrating that the specification is so vague about it.
 *
 * @{
 */

/**
 * @brief ACPI AML ObjectReference types.
 * @enum aml_object_reference_type_t
 */
typedef enum
{
    AML_OBJECT_REFERENCE_EMPTY,        //!< The object reference is not set.
    AML_OBJECT_REFERENCE_NODE,        //!< The object reference is a Node in the ACPI namespace.
    AML_OBJECT_REFERENCE_DATA_OBJECT, //!< The object reference is a DataObject.
} aml_object_reference_type_t;

/**
 * @brief ACPI AML ObjectReference structure.
 * @struct aml_object_reference_t
 */
typedef struct aml_object_reference
{
    aml_object_reference_type_t type; //!< The type of the object reference.
    union {
        aml_node_t* node;
        aml_data_object_t* dataObject;
    };
} aml_object_reference_t;

/**
 * @brief Initializes an ObjectReference to point to a node in the ACPI namespace.
 *
 * @param ref The ObjectReference to initialize.
 * @param n Pointer to the node in the ACPI namespace.
 */
#define AML_OBJECT_REFERENCE_INIT_NODE(ref, n) \
    *ref = (aml_object_reference_t){ \
        .type = AML_OBJECT_REFERENCE_NODE, .node = (n), \
    }

/**
 * @brief Initializes an ObjectReference to point to a DataObject.
 *
 * @param ref The ObjectReference to initialize.
 * @param obj Pointer to the DataObject.
 */
#define AML_OBJECT_REFERENCE_INIT_DATA_OBJECT(ref, obj) \
    *ref = (aml_object_reference_t) \
    { \
        .type = AML_OBJECT_REFERENCE_DATA_OBJECT, .dataObject = (obj), \
    }

/**
 * @brief Initializes an ObjectReference to be empty.
 *
 * @param ref The ObjectReference to initialize.
 */
#define AML_OBJECT_REFERENCE_INIT_EMPTY(ref) \
    *ref = (aml_object_reference_t) \
    { \
        .type = AML_OBJECT_REFERENCE_EMPTY, .node = NULL, \
    }

/** @} */

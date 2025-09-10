#pragma once

#include <sys/list.h>

/**
 * @brief ACPI AML Node
 * @defgroup kernel_acpi_aml_node Node
 * @ingroup kernel_acpi
 *
 * @{
 */

/**
 * @brief Maximum length of an ACPI name.
 */
#define AML_NAME_LENGTH 4

/**
 * @brief ACPI node type.
 */
typedef enum
{
    AML_NODE_NONE = 0,
    AML_NODE_PREDEFINED,
    AML_NODE_DEVICE,
    AML_NODE_PROCESSOR,
    AML_NODE_THERMAL_ZONE,
    AML_NODE_POWER_RESOURCE,
    AML_NODE_OPREGION,
    AML_NODE_MAX
} aml_node_type_t;

/**
 * @brief ACPI node.
 */
typedef struct aml_node
{
    list_entry_t entry;
    aml_node_type_t type;
    list_t children;
    struct aml_node* parent;
    char name[AML_NAME_LENGTH];
} aml_node_t;

/** @} */

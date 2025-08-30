#pragma once

/**
 * @brief Namespace management for ACPI
 * @defgroup kernel_acpi_namespace ACPI Namespaces
 * @ingroup kernel_acpi
 * @{
 */

#include "fs/sysfs.h"

#include <sys/list.h>

#include <stdint.h>

#define ACPI_NODE_NAME_LEN 4

typedef enum acpi_node_type
{
    ACPI_NODE_TYPE_ROOT,
    ACPI_NODE_TYPE_DEVICE,
} acpi_node_type_t;

#define ACPI_NODE_TYPE_IS_DIR(type) ((type) == ACPI_NODE_TYPE_DEVICE || (type) == ACPI_NODE_TYPE_ROOT)

typedef struct acpi_node
{
    list_entry_t entry;
    list_t children;
    char name[ACPI_NODE_NAME_LEN];
    acpi_node_type_t type;
} acpi_node_t;

void acpi_namespace_init(void);

/** @} */

#pragma once

#include "acpi/aml/aml.h"

#include <stdint.h>

/**
 * @brief Store to ACPI Nodes
 * @defgroup kernel_acpi_aml_store Store
 * @ingroup kernel_acpi_aml
 *
 * This module provides functionality for storing data to ACPI nodes.
 *
 * @{
 */

/**
 * @brief Store a data object in a node.
 *
 * @param node The node to store the data object in.
 * @param object The data object to store.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_store(aml_node_t* node, aml_data_object_t* object);

/** @} */

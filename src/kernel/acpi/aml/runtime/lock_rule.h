#pragma once

#include "acpi/aml/aml.h"

#include <stdint.h>

/**
 * @brief Lock Rule Handling
 * @defgroup kernel_acpi_aml_lock_rule Lock Rule
 * @ingroup kernel_acpi_aml
 *
 * This module provides functionality for handling lock rules and other synchronization related tasks.
 *
 * @{
 */

/**
 * @brief Determine if the global mutex should be acquired when accessing this node.
 *
 * The mutex should be acquired if:
 * - The node is a FieldUnit and its LockRule is `AML_LOCK_RULE_LOCK` (see section 19.6.48
 * and 19.6.64),
 * - More to be added later.
 *
 * @param node The node to check.
 * @return true If the global mutex should be acquired.
 * @return false If the global mutex should not be acquired.
 */
bool aml_should_acquire_global_mutex(aml_node_t* node);

/** @} */

#pragma once

#include "aml_node.h"

#include <errno.h>

/**
 * @brief ACPI AML Scope
 * @defgroup kernel_acpi_aml_scope Scope
 * @ingroup kernel_acpi_aml
 *
 * The `aml_scope_t` structure is used to keep track of the current scope, its passed down recursively to each function,
 * such that we do not require any state machine or similar, instead when a new scope is entered, we simply create a new
 * `aml_scope_t` and pass it to the lower function. Check `aml_def_scope_read()` for an example.
 *
 * @{
 */

/**
 * @brief ACPI AML Scope
 * @struct aml_scope_t
 */
typedef struct
{
    aml_node_t* location; //!< Current location in the AML tree
} aml_scope_t;

static inline uint64_t aml_scope_init(aml_scope_t* scope, aml_node_t* location)
{
    if (scope == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    scope->location = location;
    return 0;
}

/** @} */

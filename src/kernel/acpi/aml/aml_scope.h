#pragma once

#include "acpi/aml/aml_object.h"

#include <stdint.h>

/**
 * @brief Scope
 * @defgroup kernel_acpi_aml_scope Scope
 * @ingroup kernel_acpi_aml
 *
 * The ACPI AML scope is used to keep track of the current location in the ACPI namespace, think of it like the current
 * working directory in a filesystem.
 *
 * @{
 */

/**
 * @brief Scope structure.
 * @struct aml_scope_t
 */
typedef struct aml_scope
{
    aml_object_t* location;
} aml_scope_t;

/**
 * @brief Initialize the scope.
 *
 * @param scope The scope to initialize.
 * @param location The object to set as the current location in the namespace.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_scope_init(aml_scope_t* scope, aml_object_t* location);

/**
 * @brief Deinitialize the scope and free all temporary objects.
 *
 * @param scope The scope to deinitialize.
 */
void aml_scope_deinit(aml_scope_t* scope);

/** @} */

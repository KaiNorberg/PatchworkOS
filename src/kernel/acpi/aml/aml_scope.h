#pragma once

#include "acpi/aml/aml_object.h"

#include <stdint.h>

/**
 * @brief Scope
 * @defgroup kernel_acpi_aml_scope Scope
 * @ingroup kernel_acpi_aml
 *
 * The ACPI AML scope is used to keep track of the current location in the ACPI namespace, think of it like the current
 * working directory. It also stores temporary objects used for intermediate stuff.
 *
 * @{
 */

/**
 * @brief Number of temporary objects to allocate when more are needed.
 */
#define AML_SCOPE_TEMP_STEP 16

/**
 * @brief Scope structure.
 * @struct aml_scope_t
 *
 * Temporary objects are usefull since we cant know if, for example, a TermArg will resolve to a currently existing
 * object like a Named Object or if it will resolve to a static value like an Integer or String, so to avoid the reader
 * having the check which the TermArg is we either return the named object or a temporary object containing the static
 * value.
 */
typedef struct aml_scope
{
    aml_object_t* location;
    aml_object_t** temps;
    uint64_t tempCount;
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

/**
 * @brief Reset all temporary objects in the scope.
 *
 * @param scope The scope to reset the temporary objects in.
 */
void aml_scope_reset_temps(aml_scope_t* scope);

/**
 * @brief Get a temporary object from the scope.
 *
 * It is not needed to deinit the object after its been used as this will be done when the scope is deinitialized or
 * reset.
 *
 * @param scope The scope to get the temporary object from.
 * @return Pointer to the temporary object, or NULL on failure.
 */
aml_object_t* aml_scope_get_temp(aml_scope_t* scope);

/** @} */

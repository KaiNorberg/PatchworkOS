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
 * @brief Maximum number of temporary objects.
 */
#define AML_MAX_TEMPS (('X' - 'A' + 1) + ('9' - '0' + 1))

/**
 * @brief Convert an index to a character for naming temporary objects.
 *
 * Temporary objects are named _T_A, _T_B, ..., _T_X, _T_0, _T_1, ..., _T_9.
 *
 * @param i The index to convert.
 * @return The corresponding character.
 */
#define AML_TEMP_INDEX_TO_CHAR(i) ((i) < ('X' - 'A' + 1) ? 'A' + (i) : '0' + (i) - ('X' - 'A' + 1))

/**
 * @brief Scope structure.
 * @struct aml_scope_t
 */
typedef struct aml_scope
{
    aml_object_t* location;
    aml_object_t* temps[AML_MAX_TEMPS];
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
 * Temporary objects are named _T_A, _T_B, ..., _T_X. It is not needed to deinit the object after its been used as this
 * will be done when the scope is deinitialized or reset but sometimes its good to do it anyway to avoid running out of
 * temporary objects.
 *
 * @param scope The scope to get the temporary object from.
 * @return Pointer to the temporary object, or NULL on failure.
 */
aml_object_t* aml_scope_get_temp(aml_scope_t* scope);

/** @} */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/list.h>

#include "acpi/aml/encoding/name.h"
#include "acpi/aml/encoding/named.h"

typedef struct aml_unresolved_obj aml_unresolved_obj_t;
typedef struct aml_object aml_object_t;
typedef struct aml_data aml_data_t;

/**
 * @brief Patch-up system for forward references.
 * @defgroup kernel_acpi_aml_patch_up Patch-up
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Callback type for resolving a forward reference.
 *
 * Takes the now matched object and the previously unresolved object as parameters. The callback should patch
 * the unresolved object in whatever way it wants, for example performing type conversion or similar.
 */
typedef uint64_t (*aml_patch_up_resolve_callback_t)(aml_object_t* match, aml_object_t* unresolved);

/**
 * @brief Entry in the global list of unresolved references.
 * @struct aml_patch_up_entry_t
 */
typedef struct aml_patch_up_entry
{
    list_entry_t entry;               ///< List entry for the global list of unresolved references.
    aml_unresolved_obj_t* unresolved; ///< The unresolved object.
} aml_patch_up_entry_t;

/**
 * @brief Initialize the patch-up system.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_patch_up_init(void);

/**
 * @brief Adds a unresolved reference to the global list.
 *
 * Does not take a reference to `unresolved`, unresolved objects will remove themselves from the list when they are
 * freed.
 *
 * @param unresolved The unresolved object to add, must be of type `AML_UNRESOLVED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_patch_up_add_unresolved(aml_unresolved_obj_t* unresolved);

/**
 * @brief Removes an unresolved reference from the global list.
 *
 * @param unresolved The unresolved object to remove.
 */
void aml_patch_up_remove_unresolved(aml_unresolved_obj_t* unresolved);

/**
 * @brief Attempts to resolve all unresolved references.
 *
 * TODO: I am still not sure when would be the best time to call this function, for now its called after the DSDT and
 * all SSDTs have been loaded, i am quite sure that we will end up getting issues with unresolved references due to
 * this, but instead of trying to solve that now, we will just fix it as issues arise.
 *
 * Note that a failure to resolve a object is not considered an error, the function will just continue
 * to the next unresolved reference.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_patch_up_resolve_all(void);

/**
 * @brief Get the number of unresolved references in the global list.
 *
 * @return The number of unresolved references.
 */
uint64_t aml_patch_up_unresolved_count(void);

/** @} */

#pragma once

#include <stdint.h>

#include "acpi/aml/aml_object.h"

/**
 * @brief Predefined AML names and objects
 * @defgroup kernel_acpi_aml_predefined Predefined
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Implementation of the _OSI predefined method.
 *
 * The _OSI method is used by the ACPI firmware to query the operating system's capabilities. But
 * for now we just return true for everything.
 *
 * @See section 5.7.2 of the ACPI specification.
 *
 * @param method The _OSI method object.
 * @param argCount The number of arguments passed to the method.
 * @param args The arguments passed to the method.
 * @param returnValue The object to store the return value of the method.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_osi_implementation(aml_method_obj_t* method, uint64_t argCount, aml_object_t** args,
    aml_object_t* returnValue);

/**
 * @brief Implementation of the _REV predefined method.
 *
 * The _REV method returns the the revision of the ACPI Specification implemented by the OS.
 *
 * @see `ACPI_REVISION`
 *
 * @param method The _REV method object.
 * @param argCount The number of arguments passed to the method.
 * @param args The arguments passed to the method.
 * @param returnValue The object to store the return value of the method.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_rev_implementation(aml_method_obj_t* method, uint64_t argCount, aml_object_t** args,
    aml_object_t* returnValue);

/**
 * @brief Implementation of the _OS predefined method.
 *
 * The _OS method evaluates to a string that identifies the operating system.
 *
 * @param method The _OS method object.
 * @param argCount The number of arguments passed to the method.
 * @param args The arguments passed to the method.
 * @param returnValue The object to store the return value of the method.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_os_implementation(aml_method_obj_t* method, uint64_t argCount, aml_object_t** args,
    aml_object_t* returnValue);

/**
 * @brief Get the global AML mutex.
 *
 * @return Pointer to the global AML mutex.
 */
aml_mutex_obj_t* aml_gl_get(void);

/**
 * @brief Initialize predefined AML names and objects.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_predefined_init(void);

/** @} */

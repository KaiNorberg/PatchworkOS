#pragma once

#include <kernel/acpi/aml/object.h>
#include <sys/status.h>

#include <stdint.h>

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
 * @param args The arguments passed to the method.
 * @param argCount The number of arguments passed to the method.
 * @return On success, the return value of the method. On failure, `NULL`.
 */
aml_object_t* aml_osi_implementation(aml_method_t* method, aml_object_t** args, uint64_t argCount);

/**
 * @brief Implementation of the _REV predefined method.
 *
 * The _REV method returns the the revision of the ACPI Specification implemented by the OS.
 *
 * @see `ACPI_REVISION`
 *
 * @param method The _REV method object.
 * @param args The arguments passed to the method.
 * @param argCount The number of arguments passed to the method.
 * @return On success, the return value of the method. On failure, `NULL`.
 */
aml_object_t* aml_rev_implementation(aml_method_t* method, aml_object_t** args, uint64_t argCount);

/**
 * @brief Implementation of the _OS predefined method.
 *
 * The _OS method evaluates to a string that identifies the operating system.
 *
 * @param method The _OS method object.
 * @param args The arguments passed to the method.
 * @param argCount The number of arguments passed to the method.
 * @return On success, the return value of the method. On failure, `NULL`.
 */
aml_object_t* aml_os_implementation(aml_method_t* method, aml_object_t** args, uint64_t argCount);

/**
 * @brief Get the global AML mutex.
 *
 * @return Pointer to the global AML mutex.
 */
aml_mutex_t* aml_gl_get(void);

/**
 * @brief Initialize predefined AML names and objects.
 *
 * @return An appropriate status value.
 */
status_t aml_predefined_init(void);

/** @} */

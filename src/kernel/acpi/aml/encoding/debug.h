#pragma once

#include "acpi/aml/aml_object.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"

#include <stdint.h>

/**
 * @brief Debug Objects Encoding
 * @defgroup kernel_acpi_aml_encoding_debug Debug Objects
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Reads a DebugObj structure from the AML byte stream.
 *
 * A DebugObj structure is defined as `DebugObj := DebugOp`.
 *
 * DebugObj's are used to output debug information about objects. When a DebugObj is writen to, the source object is
 * printed to the kernel log. Its just a weird print statement.
 *
 * @see Section 19.6.26 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @return On success, the DebugObj object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_debug_obj_read(aml_state_t* state);

/** @} */

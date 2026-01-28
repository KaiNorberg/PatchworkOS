#pragma once

#include <stdint.h>
#include <sys/status.h>

typedef struct aml_object aml_object_t;
typedef struct aml_term_list_ctx aml_term_list_ctx_t;

/**
 * @brief Local Objecs Encoding
 * @defgroup kernel_acpi_aml_encoding_local Locals
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.6.2 of the ACPI specification.
 *
 * @{
 */

/**
 * @brief Maximum number of local variables that can be used in a method.
 */
#define AML_MAX_LOCALS 8

/**
 * @brief Reads a LocalObj structure from the AML byte stream.
 *
 * A LocalObj is defined as `LocalObj := Local0Op | Local1Op | Local2Op | Local3Op | Local4Op | Local5Op | Local6Op |
 * Local7Op` where
 * - Local0Op := 0x60,
 * - Local1Op := 0x61,
 * - Local2Op := 0x62,
 * - Local3Op := 0x63,
 * - Local4Op := 0x64,
 * - Local5Op := 0x65,
 * - Local6Op := 0x66 and
 * - Local7Op := 0x67.
 *
 * Note that if a LocalObj is storing a ObjectRefernce it will be Automatically dereferenced, so in such a situation
 * `out` will point to the actual object and not an ObjectReference in the LocalObj.
 *
 * @see Section 19.3.5.8.2 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out The output pointer to store the local object.
 * @return An appropriate status value.
 */
status_t aml_local_obj_read(aml_term_list_ctx_t* ctx, aml_object_t** out);

/** @} */

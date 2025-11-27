#pragma once

typedef struct aml_object aml_object_t;
typedef struct aml_term_list_ctx aml_term_list_ctx_t;

/**
 * @brief Debug Objects Encoding
 * @defgroup modules_acpi_aml_encoding_debug Debug Objects
 * @ingroup modules_acpi_aml
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
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, the DebugObj object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_debug_obj_read(aml_term_list_ctx_t* ctx);

/** @} */

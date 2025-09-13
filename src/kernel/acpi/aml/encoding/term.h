#pragma once

#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "data.h"

#include <stdint.h>

/**
 * @brief ACPI AML Term Objects Encoding
 * @defgroup kernel_acpi_aml_term Term Objects
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.5 of the ACPI specification for more details.
 *
 * Note that the `term.h` file is the only encoding file that should be included outside of the encoding directory,
 * except the `*_types.h` files. This is because we frequently use `static inline` for all encoding functions.
 *
 * @{
 */

/**
 * @brief The type of the result produced by a TermArg.
 */
typedef enum
{
    AML_TERMARG_NONE = 0,
    AML_TERMARG_INTEGER,
    AML_TERMARG_MAX,
} aml_termarg_type_t;

/**
 * @brief ACPI AML TermArg structure
 * @struct aml_termarg_t
 *
 * A TermArg structure is used to pass certain arguments to opcodes. They dont just store static information, instead
 * they are evaluated at runtime. Think of how in C you can do `myfunc(1, myotherfunc(), 2)`, in this case the
 * `myotherfunc()` argument would be a TermArg in AML.
 */
typedef struct
{
    aml_termarg_type_t type; //!< The type of the parsed result of the termarg.
    union {
        uint64_t integer;
    };
} aml_termarg_t;

/**
 * @brief Reads an TermArg structure from the AML byte stream.
 *
 * A TermArg is defined as `TermArg := ExpressionOpcode | DataObject | ArgObj | LocalObj`.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @param out The output buffer to store the result of the TermArg.
 * @param expectedType The expected type of the TermArg result, will error if a different type is encountered.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_termarg_read(aml_state_t* state, aml_scope_t* scope, aml_termarg_t* out, aml_termarg_type_t expectedType);

/**
 * @brief Wrapper for `aml_termarg_read()` with `expectedType` set to `AML_TERMARG_INTEGER`.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @param out The output buffer to store the result of the TermArg.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_termarg_read_integer(aml_state_t* state, aml_scope_t* scope, uint64_t* out);

/**
 * @brief Reads an Object structure from the AML byte stream.
 *
 * An Object is defined as `Object := NameSpaceModifierObj | NamedObj`.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_object_read(aml_state_t* state, aml_scope_t* scope);

/**
 * @brief Reads a TermObj structure from the AML byte stream.
 *
 * A TermObj is defined as `TermObj := Object | StatementOpcode | ExpressionOpcode`.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_termobj_read(aml_state_t* state, aml_scope_t* scope);

/**
 * @brief Reads a TermList structure from the AML byte stream.
 *
 * A TermList structure is defined as `TermList := Nothing | <termobj termlist>`.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @param end The index at which the termlist ends.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_termlist_read(aml_state_t* state, aml_scope_t* scope, aml_address_t end);

/** @} */

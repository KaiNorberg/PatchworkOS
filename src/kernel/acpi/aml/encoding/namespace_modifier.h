#pragma once

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "log/log.h"
#include "name.h"
#include "package_length.h"
#include "term.h"

#include <sys/list.h>

#include <errno.h>
#include <stdint.h>

/**
 * @brief ACPI AML Namespace Modifier Objects Encoding
 * @defgroup kernel_acpi_aml_namespace_modifier Namespace Modifier Objects
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.5.1 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Reads a DefScope structure from the AML byte stream.
 *
 * A DefScope structure is defined as `DefScope := ScopeOp PkgLength NameString TermList`.
 *
 * See section 19.6.122 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_def_scope_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_address_t start = state->instructionPointer;

    aml_value_t scopeOp;
    if (aml_value_read_no_ext(state, &scopeOp) == ERR)
    {
        return ERR;
    }

    if (scopeOp.num != AML_SCOPE_OP)
    {
        AML_DEBUG_INVALID_STRUCTURE("ScopeOp")
        errno = EILSEQ;
        return ERR;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_node_t* newLocation = aml_name_string_walk(&nameString, scope != NULL ? scope->location : NULL);
    if (newLocation == NULL)
    {
        return ERR;
    }

    if (newLocation->type != AML_NODE_PREDEFINED && newLocation->type != AML_NODE_DEVICE &&
        newLocation->type != AML_NODE_PROCESSOR && newLocation->type != AML_NODE_THERMAL_ZONE &&
        newLocation->type != AML_NODE_POWER_RESOURCE)
    {
        AML_DEBUG_INVALID_STRUCTURE("NameString")
        errno = EILSEQ;
        return ERR;
    }

    aml_scope_t newScope;
    if (aml_scope_init(&newScope, newLocation) == ERR)
    {
        return ERR;
    }

    return aml_termlist_read(state, &newScope, end);
}

/**
 * @brief Reads a NameSpaceModifierObj structure from the AML byte stream.
 *
 * A NameSpaceModifierObj structure is defined as `NameSpaceModifierObj := DefAlias | DefName | DefScope`.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @param op The AML op, should have been read by the caller.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_namespace_modifier_obj_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_ALIAS_OP:
        AML_DEBUG_UNEXPECTED_VALUE(&value);
        errno = EILSEQ;
        return ERR;
    case AML_NAME_OP:
        AML_DEBUG_UNEXPECTED_VALUE(&value);
        errno = EILSEQ;
        return ERR;
    case AML_SCOPE_OP:
        return aml_def_scope_read(state, scope);
    default:
        AML_DEBUG_UNEXPECTED_VALUE(&value);
        errno = EILSEQ;
        return ERR;
    }
}

/** @} */

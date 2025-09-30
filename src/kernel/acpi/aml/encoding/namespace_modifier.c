#include "namespace_modifier.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "name.h"
#include "package_length.h"
#include "term.h"

#include <sys/list.h>

#include <errno.h>
#include <stdint.h>

uint64_t aml_def_alias_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t aliasOp;
    if (aml_value_read_no_ext(state, &aliasOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read AliasOp");
        return ERR;
    }

    if (aliasOp.num != AML_ALIAS_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid AliasOp '0x%x'", aliasOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* source = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &source, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve source NameString");
        return ERR;
    }

    aml_name_string_t targetNameString;
    if (aml_name_string_read(state, &targetNameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve target NameString");
        return ERR;
    }

    aml_node_t* target = aml_node_add(scope->node, &targetNameString, AML_NODE_NONE);
    if (target == NULL)
    {
        errno = EILSEQ;
        return ERR;
    }

    if (aml_node_init_alias(target, source) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_def_name_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t nameOp;
    if (aml_value_read_no_ext(state, &nameOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameOp");
        return ERR;
    }

    if (nameOp.num != AML_NAME_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid NameOp '0x%x'", nameOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameString");
        return ERR;
    }

    aml_node_t* name = aml_node_add(scope->node, &nameString, AML_NODE_NONE);
    if (name == NULL)
    {
        errno = EILSEQ;
        return ERR;
    }

    if (aml_data_ref_object_read(state, scope, name) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DataRefObject");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_scope_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t scopeOp;
    if (aml_value_read_no_ext(state, &scopeOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ScopeOp");
        return ERR;
    }

    if (scopeOp.num != AML_SCOPE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ScopeOp '0x%x'", scopeOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLength");
        return ERR;
    }

    aml_node_t* newLocation = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &newLocation, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve NameString");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    if (newLocation->type != AML_DATA_DEVICE && newLocation->type != AML_DATA_PROCESSOR &&
        newLocation->type != AML_DATA_THERMAL_ZONE && newLocation->type != AML_DATA_POWER_RESOURCE)
    {
        AML_DEBUG_ERROR(state, "Invalid node type '%s'", aml_data_type_to_string(newLocation->type));
        errno = EILSEQ;
        return ERR;
    }

    if (aml_term_list_read(state, newLocation, end) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermList");
        return ERR;
    }

    return 0;
}

uint64_t aml_namespace_modifier_obj_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    switch (value.num)
    {
    case AML_ALIAS_OP:
        if (aml_def_alias_read(state, scope) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read DefAlias");
            return ERR;
        }
        return 0;
    case AML_NAME_OP:
        if (aml_def_name_read(state, scope) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read DefName");
            return ERR;
        }
        return 0;
    case AML_SCOPE_OP:
        if (aml_def_scope_read(state, scope) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read DefScope");
            return ERR;
        }
        return 0;
    default:
        AML_DEBUG_ERROR(state, "Invalid NamespaceModifierObj '0x%x'", value.num);
        errno = EILSEQ;
        return ERR;
    }
}

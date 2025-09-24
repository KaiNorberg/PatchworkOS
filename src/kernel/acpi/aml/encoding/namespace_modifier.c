#include "namespace_modifier.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "name.h"
#include "package_length.h"
#include "term.h"

#include <sys/list.h>

#include <errno.h>
#include <stdint.h>

uint64_t aml_def_name_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t nameOp;
    if (aml_value_read_no_ext(state, &nameOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read name op");
        return ERR;
    }

    if (nameOp.num != AML_NAME_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid name op: 0x%x", nameOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read name string");
        return ERR;
    }

    aml_node_t* name = aml_node_add(&nameString, node, AML_NODE_NONE);
    if (name == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to add node");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_data_ref_object_read(state, node, name) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read data ref object");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_scope_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t scopeOp;
    if (aml_value_read_no_ext(state, &scopeOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read scope op");
        return ERR;
    }

    if (scopeOp.num != AML_SCOPE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid scope op: 0x%x", scopeOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_address_t start = state->pos;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pkg length");
        return ERR;
    }

    aml_node_t* scope = NULL;
    if (aml_name_string_read_and_resolve(state, node, &scope) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve scope name string");
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    if (scope->type != AML_DATA_DEVICE && scope->type != AML_DATA_PROCESSOR && scope->type != AML_DATA_THERMAL_ZONE &&
        scope->type != AML_DATA_POWER_RESOURCE)
    {
        AML_DEBUG_ERROR(state, "Invalid node type '%s'", aml_data_type_to_string(scope->type));
        errno = EILSEQ;
        return ERR;
    }

    if (aml_term_list_read(state, scope, end) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term list");
        return ERR;
    }

    return 0;
}

uint64_t aml_namespace_modifier_obj_read(aml_state_t* state, aml_node_t* node)
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
        AML_DEBUG_ERROR(state, "Alias op is not implemented");
        errno = EILSEQ;
        return ERR;
    case AML_NAME_OP:
        if (aml_def_name_read(state, node) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read DefName");
            return ERR;
        }
        return 0;
    case AML_SCOPE_OP:
        if (aml_def_scope_read(state, node) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read DefScope");
            return ERR;
        }
        return 0;
    default:
        AML_DEBUG_ERROR(state, "Invalid namespace modifier obj: 0x%x", value.num);
        errno = EILSEQ;
        return ERR;
    }
}

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

    aml_node_t* name = aml_node_add(&nameString, node, AML_NODE_NAME);
    if (name == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to add node");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_data_ref_object_read(state, &name->name.object) == ERR)
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

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read name string");
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_node_t* newNode = aml_node_find(&nameString, node);
    if (newNode == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to walk '%s' from '%s'\n", aml_name_string_to_string(&nameString),
            node->segment);
        errno = EILSEQ;
        return ERR;
    }

    if (newNode->type != AML_NODE_PREDEFINED && newNode->type != AML_NODE_DEVICE &&
        newNode->type != AML_NODE_PROCESSOR && newNode->type != AML_NODE_THERMAL_ZONE &&
        newNode->type != AML_NODE_POWER_RESOURCE)
    {
        AML_DEBUG_ERROR(state, "Invalid node type: %d", newNode->type);
        errno = EILSEQ;
        return ERR;
    }

    return aml_term_list_read(state, newNode, end);
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
        return aml_def_name_read(state, node);
    case AML_SCOPE_OP:
        return aml_def_scope_read(state, node);
    default:
        AML_DEBUG_ERROR(state, "Invalid namespace modifier obj: 0x%x", value.num);
        errno = EILSEQ;
        return ERR;
    }
}

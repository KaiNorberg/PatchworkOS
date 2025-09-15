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
        return ERR;
    }

    if (nameOp.num != AML_NAME_OP)
    {
        AML_DEBUG_INVALID_STRUCTURE("NameOp");
        errno = EILSEQ;
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        return ERR;
    }

    aml_node_t* name = aml_add_node_at_name_string(&nameString, node, AML_NODE_NAME);
    if (name == NULL)
    {
        AML_DEBUG_INVALID_STRUCTURE("NameString");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_data_ref_object_read(state, &name->data.name.object) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_def_scope_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t scopeOp;
    if (aml_value_read_no_ext(state, &scopeOp) == ERR)
    {
        return ERR;
    }

    if (scopeOp.num != AML_SCOPE_OP)
    {
        AML_DEBUG_INVALID_STRUCTURE("ScopeOp");
        errno = EILSEQ;
        return ERR;
    }

    aml_address_t start = state->pos;

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

    aml_node_t* newNode = aml_find_node_name_string(&nameString, node);
    if (newNode == NULL)
    {
        LOG_ERR("failed to walk '%s' from '%s'\n", aml_name_string_to_string(&nameString),
            node != NULL ? node->name : "\\___");
        AML_DEBUG_INVALID_STRUCTURE("NameString: Could not find node");
        errno = EILSEQ;
        return ERR;
    }

    if (newNode->type != AML_NODE_PREDEFINED && newNode->type != AML_NODE_DEVICE &&
        newNode->type != AML_NODE_PROCESSOR && newNode->type != AML_NODE_THERMAL_ZONE &&
        newNode->type != AML_NODE_POWER_RESOURCE)
    {
        AML_DEBUG_INVALID_STRUCTURE("NameString: Node is of a invalid type");
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
        return ERR;
    }

    switch (value.num)
    {
    case AML_ALIAS_OP:
        AML_DEBUG_UNEXPECTED_VALUE(&value);
        errno = EILSEQ;
        return ERR;
    case AML_NAME_OP:
        return aml_def_name_read(state, node);
    case AML_SCOPE_OP:
        return aml_def_scope_read(state, node);
    default:
        AML_DEBUG_UNEXPECTED_VALUE(&value);
        errno = EILSEQ;
        return ERR;
    }
}

#include "namespace_modifier.h"

#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_op.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "log/log.h"
#include "name.h"
#include "package_length.h"
#include "term.h"

#include <sys/list.h>

#include <errno.h>
#include <stdint.h>

uint64_t aml_def_alias_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op)
{
    LOG_ERR("DefAlias not implemented\n");
    errno = ENOTSUP;
    return ERR;
}

uint64_t aml_def_name_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op)
{
    LOG_ERR("DefName not implemented\n");
    errno = ENOTSUP;
    return ERR;
}

uint64_t aml_def_scope_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op)
{
    uint64_t start = state->instructionPointer;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    uint64_t end = start + pkgLength - op->length;

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        return ERR;
    }

    aml_node_t* newLocation = aml_name_string_walk(&nameString, scope != NULL ? scope->location : NULL);
    if (newLocation == NULL)
    {
        return ERR;
    }

    if (newLocation->type != AML_NODE_PREDEFINED && newLocation->type != AML_NODE_DEVICE &&
        newLocation->type != AML_NODE_PROCESSOR && newLocation->type != AML_NODE_THERMAL_ZONE &&
        newLocation->type != AML_NODE_POWER_RESOURCE)
    {
        errno = ENOENT;
        return ERR;
    }

    aml_scope_t newScope;
    if (aml_scope_init(&newScope, newLocation) == ERR)
    {
        return ERR;
    }

    return aml_termlist_read(state, &newScope, end);
}

uint64_t aml_namespace_modifier_obj_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op)
{
    switch (op->num)
    {
    case AML_OP_ALIAS:
        return aml_def_alias_read(state, scope, op);
    case AML_OP_NAME:
        return aml_def_name_read(state, scope, op);
    case AML_OP_SCOPE:
        return aml_def_scope_read(state, scope, op);
    default:
        LOG_ERR("Unexpected opcode in aml_namespace_modifier_obj_read() (%s, 0x%.4x)\n", op->props->name, op->num);
        errno = EILSEQ;
        return ERR;
    }
}

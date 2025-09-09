#include "term.h"

#include "acpi/aml/aml_op.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_scope.h"
#include "log/log.h"
#include "namespace_modifier.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_object_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_op_t op;
    if (aml_op_read(state, &op, AML_OP_FLAG_NAMESPACE_MODIFIER | AML_OP_FLAG_NAMED) == ERR)
    {
        LOG_ERR("Unexpected opcode in aml_object_read() (%s, 0x%.4x)\n", op.props->name, op.num);
        errno = EILSEQ;
        return ERR;
    }

    if (op.props->flags & AML_OP_FLAG_NAMESPACE_MODIFIER)
    {
        return aml_namespace_modifier_obj_read(state, scope, &op);
    }
    else if (op.props->flags & AML_OP_FLAG_NAMED)
    {
        // TODO: Implement named object parsing.
        LOG_ERR("Named object parsing not implemented\n");
        errno = ENOTSUP;
        return ERR;
    }

    // This really should not happen as invalid opcodes should be caught in the aml_op_read() == ERR check.
    LOG_ERR("Parser error in aml_object_read()\n");
    errno = EPROTO;
    return ERR;
}

uint64_t aml_termobj_read(aml_state_t* state, aml_scope_t* scope)
{
    // Attempt to read a statement or expression opcode, if it fails, then its probably an object. Note that an object
    // is technically also defined using opcodes which can be bit confusing.
    aml_op_t op;
    if (aml_op_read(state, &op, AML_OP_FLAG_STATEMENT | AML_OP_FLAG_EXPRESSION) == ERR)
    {
        return aml_object_read(state, scope);
    }

    if (op.props->flags & AML_OP_FLAG_STATEMENT)
    {
        LOG_ERR("Statement opcode not implemented (%s, 0x%.4x)\n", op.props->name, op.num);
        errno = ENOSYS;
        return ERR;
    }

    if (op.props->flags & AML_OP_FLAG_EXPRESSION)
    {
        LOG_ERR("Expression opcode not implemented (%s, 0x%.4x)\n", op.props->name, op.num);
        errno = ENOSYS;
        return ERR;
    }

    LOG_ERR("Unexpected opcode in aml_termobj_read() (%s, 0x%.4x)\n", op.props->name, op.num);
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_termlist_read(aml_state_t* state, aml_scope_t* scope, uint64_t end)
{
    while (end > state->instructionPointer)
    {
        // No more data available even though we are not at the end of the buffer.
        if (aml_byte_peek(state) == ERR)
        {
            return ERR;
        }

        // End of buffer not reached => byte is not nothing => must be a termobj.
        if (aml_termobj_read(state, scope) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

#include "named.h"

#include "log/log.h"

uint64_t aml_def_op_region_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op)
{
    LOG_ERR("DefOpRegion not implemented\n");
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_named_obj_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op)
{
    switch (op->num)
    {
    case AML_OP_OPREGION:
        return aml_def_op_region_read(state, scope, op);
    default:
        LOG_ERR("Unsupported opcode in aml_named_object_read() (%s, 0x%.4x)\n", op->props->name, op->num);
        errno = ENOSYS;
        return ERR;
    }
}

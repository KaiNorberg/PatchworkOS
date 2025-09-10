#include "named.h"
#include "name.h"
#include "data.h"
#include "term.h"
#include "acpi/aml/aml_state.h"

#include "log/log.h"

uint64_t aml_region_space_read(aml_state_t* state, aml_region_space_t* out)
{
    aml_byte_data_t byteData;
    if (aml_byte_data_read(state, &byteData) == ERR)
    {
        return ERR;
    }
 
    if (byteData > AML_REGION_PCC && byteData < AML_REGION_OEM_MIN)
    {
        errno = EILSEQ;
        return ERR;
    }

    *out = byteData;
    return 0;
}

uint64_t aml_region_offset_read(aml_state_t* state, aml_scope_t* scope, aml_region_offset_t* out)
{
    return aml_termarg_integer_read(state, scope, out);
}

uint64_t aml_region_len_read(aml_state_t* state, aml_scope_t* scope, aml_region_len_t* out)
{
    return aml_termarg_integer_read(state, scope, out);
}

uint64_t aml_def_op_region_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op)
{
    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        return ERR;
    }

    aml_region_space_t regionSpace;
    if (aml_region_space_read(state, &regionSpace) == ERR)
    {
        return ERR;
    }

    aml_region_offset_t regionOffset;
    if (aml_region_offset_read(state, scope, &regionOffset) == ERR)
    {
        return ERR;
    }

    aml_region_len_t regionLen;
    if (aml_region_len_read(state, scope, &regionLen) == ERR)
    {
        return ERR;
    }

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

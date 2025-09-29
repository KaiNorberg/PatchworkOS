#include "named.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "data.h"
#include "name.h"
#include "package_length.h"
#include "term.h"

uint64_t aml_bank_value_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_region_space_read(aml_state_t* state, aml_region_space_t* out)
{
    uint8_t byte;
    if (aml_byte_data_read(state, &byte) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ByteData");
        return ERR;
    }

    if (byte > AML_REGION_PCC && byte < AML_REGION_OEM_MIN)
    {
        AML_DEBUG_ERROR(state, "Invalid RegionSpace: '0x%x'", byte);
        errno = EILSEQ;
        return ERR;
    }

    *out = (aml_region_space_t)byte;
    return 0;
}

uint64_t aml_region_offset_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_region_len_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_op_region_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t opRegionOp;
    if (aml_value_read(state, &opRegionOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read OpRegionOp");
        return ERR;
    }

    if (opRegionOp.num != AML_OPREGION_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid OpRegionOp '0x%x'", opRegionOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameString");
        return ERR;
    }

    aml_region_space_t regionSpace;
    if (aml_region_space_read(state, &regionSpace) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read RegionSpace");
        return ERR;
    }

    uint64_t regionOffset;
    if (aml_region_offset_read(state, scope, &regionOffset) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read RegionOffset");
        return ERR;
    }

    uint64_t regionLen;
    if (aml_region_len_read(state, scope, &regionLen) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read RegionLen");
        return ERR;
    }

    aml_node_t* newNode = aml_node_add(&nameString, scope->node, AML_NODE_NONE);
    if (newNode == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to add node '%s'", aml_name_string_to_string(&nameString));
        return ERR;
    }

    if (aml_node_init_opregion(newNode, regionSpace, regionOffset, regionLen) == ERR)
    {
        aml_node_free(newNode);
        AML_DEBUG_ERROR(state, "Failed to init opregion");
        return ERR;
    }

    return 0;
}

uint64_t aml_field_flags_read(aml_state_t* state, aml_field_flags_t* out)
{
    uint8_t flags;
    if (aml_byte_data_read(state, &flags) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ByteData");
        return ERR;
    }

    if (flags & (1 << 7))
    {
        AML_DEBUG_ERROR(state, "Reserved bit 7 is set in FieldFlags '0x%x'", flags);
        errno = EILSEQ;
        return ERR;
    }

    aml_access_type_t accessType = flags & 0xF;
    if (accessType > AML_ACCESS_TYPE_BUFFER)
    {
        AML_DEBUG_ERROR(state, "Invalid AccessType in FieldFlags '0x%x'", accessType);
        errno = EILSEQ;
        return ERR;
    }

    *out = (aml_field_flags_t){
        .accessType = accessType,
        .lockRule = (flags >> 4) & 0x1,
        .updateRule = (flags >> 5) & 0x3,
    };

    return 0;
}

uint64_t aml_named_field_read(aml_state_t* state, aml_scope_t* scope, aml_field_list_ctx_t* ctx)
{
    aml_name_seg_t* name;
    if (aml_name_seg_read(state, &name) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameSeg");
        return ERR;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLength");
        return ERR;
    }

    switch (ctx->type)
    {
    case AML_FIELD_LIST_TYPE_FIELD:
    {
        if (ctx->field.opregion == NULL)
        {
            AML_DEBUG_ERROR(state, "opregion is null");
            errno = EILSEQ;
            return ERR;
        }

        aml_node_t* newNode = aml_node_new(scope->node, name->name, AML_NODE_NONE);
        if (newNode == NULL)
        {
            return ERR;
        }

        if (aml_node_init_field_unit_field(newNode, ctx->field.opregion, ctx->flags, ctx->currentOffset, pkgLength) ==
            ERR)
        {
            aml_node_free(newNode);
            return ERR;
        }
    }
    break;
    case AML_FIELD_LIST_TYPE_INDEX_FIELD:
    {
        if (ctx->index.indexNode == NULL)
        {
            AML_DEBUG_ERROR(state, "indexNode is null");
            errno = EILSEQ;
            return ERR;
        }

        if (ctx->index.indexNode->type != AML_DATA_FIELD_UNIT ||
            ctx->index.indexNode->fieldUnit.type != AML_FIELD_UNIT_FIELD)
        {
            AML_DEBUG_ERROR(state, "indexNode is not a field");
            errno = EILSEQ;
            return ERR;
        }

        if (ctx->index.dataNode == NULL)
        {
            AML_DEBUG_ERROR(state, "dataNode is null");
            errno = EILSEQ;
            return ERR;
        }

        aml_node_t* newNode = aml_node_new(scope->node, name->name, AML_NODE_NONE);
        if (newNode == NULL)
        {
            return ERR;
        }

        if (aml_node_init_field_unit_index_field(newNode, ctx->index.indexNode, ctx->index.dataNode, ctx->flags,
                ctx->currentOffset, pkgLength) == ERR)
        {
            aml_node_free(newNode);
            return ERR;
        }
    }
    break;
    case AML_FIELD_LIST_TYPE_BANK_FIELD:
    {
        if (ctx->bank.opregion == NULL)
        {
            AML_DEBUG_ERROR(state, "opregion is null");
            errno = EILSEQ;
            return ERR;
        }

        if (ctx->bank.bank == NULL)
        {
            AML_DEBUG_ERROR(state, "bank is null");
            errno = EILSEQ;
            return ERR;
        }

        aml_node_t* newNode = aml_node_new(scope->node, name->name, AML_NODE_NONE);
        if (newNode == NULL)
        {
            return ERR;
        }

        if (aml_node_init_field_unit_bank_field(newNode, ctx->bank.opregion, ctx->bank.bank, ctx->bank.bankValue,
                ctx->flags, ctx->currentOffset, pkgLength) == ERR)
        {
            aml_node_free(newNode);
            return ERR;
        }
    }
    break;
    default:
        AML_DEBUG_ERROR(state, "Invalid FieldList type '%d'", ctx->type);
        errno = EILSEQ;
        return ERR;
    }

    ctx->currentOffset += pkgLength;
    return 0;
}

uint64_t aml_reserved_field_read(aml_state_t* state, aml_field_list_ctx_t* ctx)
{
    aml_value_t value;
    if (aml_value_read(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ReservedField");
        return ERR;
    }

    if (value.num != 0x00)
    {
        AML_DEBUG_ERROR(state, "Invalid ReservedField value '0x%x'", value.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLength");
        return ERR;
    }

    ctx->currentOffset += pkgLength;
    return 0;
}

uint64_t aml_field_element_read(aml_state_t* state, aml_scope_t* scope, aml_field_list_ctx_t* ctx)
{
    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    if (AML_IS_LEAD_NAME_CHAR(&value))
    {
        if (aml_named_field_read(state, scope, ctx) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read NamedField");
            return ERR;
        }
    }
    else if (value.num == 0x00)
    {
        if (aml_reserved_field_read(state, ctx) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ReservedField");
            return ERR;
        }
    }
    else
    {
        AML_DEBUG_ERROR(state, "Invalid field element value '0x%x'", value.num);
        errno = ENOSYS;
        return ERR;
    }

    return 0;
}

uint64_t aml_field_list_read(aml_state_t* state, aml_scope_t* scope, aml_field_list_ctx_t* ctx, const uint8_t* end)
{
    while (end > state->current)
    {
        // End of buffer not reached => byte is not nothing => must be a FieldElement.
        if (aml_field_element_read(state, scope, ctx) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read field element");
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_def_field_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t fieldOp;
    if (aml_value_read(state, &fieldOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read field op");
        return ERR;
    }

    if (fieldOp.num != AML_FIELD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid field op: 0x%x", fieldOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pkg length");
        return ERR;
    }

    aml_node_t* opregion = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &opregion, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve name string");
        return ERR;
    }

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(state, &fieldFlags) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read field flags");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    aml_field_list_ctx_t ctx = {
        .type = AML_FIELD_LIST_TYPE_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .field.opregion = opregion,
    };

    if (aml_field_list_read(state, scope, &ctx, end) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read field list");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_index_field_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t indexFieldOp;
    if (aml_value_read(state, &indexFieldOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read index field op");
        return ERR;
    }

    if (indexFieldOp.num != AML_INDEX_FIELD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid index field op: 0x%x", indexFieldOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pkg length");
        return ERR;
    }

    aml_node_t* indexNode = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &indexNode, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve index name string");
        return ERR;
    }

    aml_node_t* dataNode = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &dataNode, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve data name string");
        return ERR;
    }

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(state, &fieldFlags) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read field flags");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    aml_field_list_ctx_t ctx = {
        .type = AML_FIELD_LIST_TYPE_INDEX_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .index.indexNode = indexNode,
        .index.dataNode = dataNode,
    };

    if (aml_field_list_read(state, scope, &ctx, end) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read field list");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_bank_field_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t value;
    if (aml_value_read(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BankFieldOp");
        return ERR;
    }

    if (value.num != AML_BANK_FIELD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid BankFieldOp '0x%x'", value.num);
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

    const uint8_t* end = start + pkgLength;

    aml_node_t* opregion = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &opregion, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve opregion NameString");
        return ERR;
    }

    aml_node_t* bank = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &bank, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve bank NameString");
        return ERR;
    }

    uint64_t bankValue;
    if (aml_bank_value_read(state, scope, &bankValue) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BankValue");
        return ERR;
    }

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(state, &fieldFlags) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read FieldFlags");
        return ERR;
    }

    aml_field_list_ctx_t ctx = {
        .type = AML_FIELD_LIST_TYPE_BANK_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .bank.opregion = opregion,
        .bank.bank = bank,
        .bank.bankValue = bankValue,
    };

    if (aml_field_list_read(state, scope, &ctx, end) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read FieldList");
        return ERR;
    }

    return 0;
}

uint64_t aml_method_flags_read(aml_state_t* state, aml_method_flags_t* out)
{
    uint8_t flags;
    if (aml_byte_data_read(state, &flags) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ByteData");
        return ERR;
    }

    uint8_t argCount = flags & 0x7;
    bool isSerialized = (flags >> 3) & 0x1;
    uint8_t syncLevel = (flags >> 4) & 0xF;

    *out = (aml_method_flags_t){
        .argCount = argCount,
        .isSerialized = isSerialized,
        .syncLevel = syncLevel,
    };

    return 0;
}

uint64_t aml_def_method_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t methodOp;
    if (aml_value_read_no_ext(state, &methodOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read MethodOp");
        return ERR;
    }

    if (methodOp.num != AML_METHOD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid MethodOp '0x%x'", methodOp.num);
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

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameString");
        return ERR;
    }

    aml_method_flags_t methodFlags;
    if (aml_method_flags_read(state, &methodFlags) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read MethodFlags");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    aml_node_t* newNode = aml_node_add(&nameString, scope->node, AML_NODE_NONE);
    if (newNode == NULL)
    {
        return ERR;
    }

    if (aml_node_init_method(newNode, &methodFlags, state->current, end, NULL) == ERR)
    {
        aml_node_free(newNode);
        return ERR;
    }

    // We are only defining the method, not executing it, so we skip its body, and only parse it when it is called.
    uint64_t offset = end - state->current;
    aml_state_advance(state, offset);

    return 0;
}

uint64_t aml_def_device_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t deviceOp;
    if (aml_value_read(state, &deviceOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read device op");
        return ERR;
    }

    if (deviceOp.num != AML_DEVICE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid device op: 0x%x", deviceOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

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

    const uint8_t* end = start + pkgLength;

    aml_node_t* newNode = aml_node_add(&nameString, scope->node, AML_NODE_NONE);
    if (newNode == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to add node");
        return ERR;
    }

    if (aml_node_init_device(newNode) == ERR)
    {
        aml_node_free(newNode);
        AML_DEBUG_ERROR(state, "Failed to init device");
        return ERR;
    }

    return aml_term_list_read(state, newNode, end);
}

uint64_t aml_sync_flags_read(aml_state_t* state, aml_sync_level_t* out)
{
    uint8_t flags;
    if (aml_byte_data_read(state, &flags) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ByteData");
        return ERR;
    }

    if (flags & 0xF0)
    {
        AML_DEBUG_ERROR(state, "Reserved bits are set in SyncFlags '0x%x'", flags);
        errno = EILSEQ;
        return ERR;
    }

    *out = flags & 0x0F;
    return 0;
}

uint64_t aml_def_mutex_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t mutexOp;
    if (aml_value_read(state, &mutexOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read MutexOp");
        return ERR;
    }

    if (mutexOp.num != AML_MUTEX_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid MutexOp '0x%x'", mutexOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameString");
        return ERR;
    }

    aml_sync_level_t syncFlags;
    if (aml_sync_flags_read(state, &syncFlags) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read SyncFlags");
        return ERR;
    }

    aml_node_t* newNode = aml_node_add(&nameString, scope->node, AML_NODE_NONE);
    if (newNode == NULL)
    {
        return ERR;
    }

    if (aml_node_init_mutex(newNode, syncFlags) == ERR)
    {
        aml_node_free(newNode);
        return ERR;
    }

    return 0;
}

uint64_t aml_proc_id_read(aml_state_t* state, aml_proc_id_t* out)
{
    if (aml_byte_data_read(state, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ByteData");
        return ERR;
    }
    return 0;
}

uint64_t aml_pblk_addr_read(aml_state_t* state, aml_pblk_addr_t* out)
{
    if (aml_dword_data_read(state, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DWordData");
        return ERR;
    }
    return 0;
}

uint64_t aml_pblk_len_read(aml_state_t* state, aml_pblk_len_t* out)
{
    if (aml_byte_data_read(state, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ByteData");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_processor_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t processorOp;
    if (aml_value_read(state, &processorOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read processor op");
        return ERR;
    }

    if (processorOp.num != AML_DEPRECATED_PROCESSOR_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid processor op: 0x%x", processorOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

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

    aml_proc_id_t procId;
    if (aml_proc_id_read(state, &procId) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read proc id");
        return ERR;
    }

    aml_pblk_addr_t pblkAddr;
    if (aml_pblk_addr_read(state, &pblkAddr) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pblk addr");
        return ERR;
    }

    aml_pblk_len_t pblkLen;
    if (aml_pblk_len_read(state, &pblkLen) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pblk len");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    aml_node_t* newNode = aml_node_add(&nameString, scope->node, AML_NODE_NONE);
    if (newNode == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to add node");
        return ERR;
    }

    if (aml_node_init_processor(newNode, procId, pblkAddr, pblkLen) == ERR)
    {
        aml_node_free(newNode);
        AML_DEBUG_ERROR(state, "Failed to init processor");
        return ERR;
    }

    return aml_term_list_read(state, newNode, end);
}

uint64_t aml_source_buff_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    if (aml_term_arg_read(state, scope, out, AML_DATA_BUFFER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_bit_index_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_byte_index_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_create_bit_field_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t value;
    if (aml_value_read_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (value.num != AML_CREATE_BIT_FIELD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid create bit field op: 0x%x", value.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* sourceBuff = NULL;
    if (aml_source_buff_read(state, scope, &sourceBuff) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read SourceBuff");
        return ERR;
    }

    assert(sourceBuff->type == AML_DATA_BUFFER);

    uint64_t bitIndex;
    if (aml_bit_index_read(state, scope, &bitIndex) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BitIndex");
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameString");
        return ERR;
    }

    aml_node_t* newNode = aml_node_add(&nameString, scope->node, AML_NODE_NONE);
    if (newNode == NULL)
    {
        return ERR;
    }

    if (aml_node_init_buffer_field(newNode, sourceBuff->buffer.content, bitIndex, 1) == ERR)
    {
        aml_node_free(newNode);
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_def_create_field_read_helper(aml_state_t* state, aml_scope_t* scope, uint8_t fieldWidth,
    aml_value_num_t expectedOp)
{
    aml_value_t value;
    if (aml_value_read_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (value.num != expectedOp)
    {
        AML_DEBUG_ERROR(state, "Invalid create field op '0x%x'", value.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* sourceBuff = NULL;
    if (aml_source_buff_read(state, scope, &sourceBuff) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read SourceBuff");
        return ERR;
    }

    assert(sourceBuff->type == AML_DATA_BUFFER);

    uint64_t byteIndex;
    if (aml_byte_index_read(state, scope, &byteIndex) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ByteIndex");
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameString");
        return ERR;
    }

    aml_node_t* newNode = aml_node_add(&nameString, scope->node, AML_NODE_NONE);
    if (newNode == NULL)
    {
        return ERR;
    }

    if (aml_node_init_buffer_field(newNode, sourceBuff->buffer.content, byteIndex * 8, fieldWidth) == ERR)
    {
        aml_node_free(newNode);
        return ERR;
    }

    return 0;
}

uint64_t aml_def_create_byte_field_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_def_create_field_read_helper(state, scope, 8, AML_CREATE_BYTE_FIELD_OP);
}

uint64_t aml_def_create_word_field_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_def_create_field_read_helper(state, scope, 16, AML_CREATE_WORD_FIELD_OP);
}

uint64_t aml_def_create_dword_field_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_def_create_field_read_helper(state, scope, 32, AML_CREATE_DWORD_FIELD_OP);
}

uint64_t aml_def_create_qword_field_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_def_create_field_read_helper(state, scope, 64, AML_CREATE_QWORD_FIELD_OP);
}

uint64_t aml_named_obj_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t op;
    if (aml_value_peek(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    uint64_t result = 0;
    switch (op.num)
    {
    case AML_OPREGION_OP:
        result = aml_def_op_region_read(state, scope);
        break;
    case AML_FIELD_OP:
        result = aml_def_field_read(state, scope);
        break;
    case AML_METHOD_OP:
        result = aml_def_method_read(state, scope);
        break;
    case AML_DEVICE_OP:
        result = aml_def_device_read(state, scope);
        break;
    case AML_MUTEX_OP:
        result = aml_def_mutex_read(state, scope);
        break;
    case AML_INDEX_FIELD_OP:
        result = aml_def_index_field_read(state, scope);
        break;
    case AML_BANK_FIELD_OP:
        result = aml_def_bank_field_read(state, scope);
        break;
    case AML_DEPRECATED_PROCESSOR_OP:
        result = aml_def_processor_read(state, scope);
        break;
    case AML_CREATE_BIT_FIELD_OP:
        result = aml_def_create_bit_field_read(state, scope);
        break;
    case AML_CREATE_BYTE_FIELD_OP:
        result = aml_def_create_byte_field_read(state, scope);
        break;
    case AML_CREATE_WORD_FIELD_OP:
        result = aml_def_create_word_field_read(state, scope);
        break;
    case AML_CREATE_DWORD_FIELD_OP:
        result = aml_def_create_dword_field_read(state, scope);
        break;
    case AML_CREATE_QWORD_FIELD_OP:
        result = aml_def_create_qword_field_read(state, scope);
        break;
    default:
        AML_DEBUG_ERROR(state, "Unknown NamedObj '0x%x'", op.num);
        errno = ENOSYS;
        return ERR;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read opcode '0x%x'", op.num);
        return ERR;
    }

    return 0;
}

/** @} */

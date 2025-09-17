#include "named.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "data.h"
#include "name.h"
#include "package_length.h"
#include "term.h"

uint64_t aml_region_space_read(aml_state_t* state, aml_region_space_t* out)
{
    aml_byte_data_t byteData;
    if (aml_byte_data_read(state, &byteData) == ERR)
    {
        return ERR;
    }

    if (byteData > AML_REGION_PCC && byteData < AML_REGION_OEM_MIN)
    {
        AML_DEBUG_INVALID_STRUCTURE("ByteData");
        errno = EILSEQ;
        return ERR;
    }

    *out = byteData;
    return 0;
}

uint64_t aml_region_offset_read(aml_state_t* state, aml_node_t* node, aml_region_offset_t* out)
{
    aml_data_object_t termArg;
    if (aml_term_arg_read(state, node, &termArg, AML_DATA_INTEGER) == ERR)
    {
        return ERR;
    }

    *out = termArg.integer;
    aml_data_object_deinit(&termArg);

    return 0;
}

uint64_t aml_region_len_read(aml_state_t* state, aml_node_t* node, aml_region_len_t* out)
{
    aml_data_object_t termArg;
    if (aml_term_arg_read(state, node, &termArg, AML_DATA_INTEGER) == ERR)
    {
        return ERR;
    }

    *out = termArg.integer;
    aml_data_object_deinit(&termArg);

    return 0;
}

uint64_t aml_def_op_region_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t opRegionOp;
    if (aml_value_read(state, &opRegionOp) == ERR)
    {
        return ERR;
    }

    if (opRegionOp.num != AML_OPREGION_OP)
    {
        AML_DEBUG_INVALID_STRUCTURE("OpRegionOp");
        errno = EILSEQ;
        return ERR;
    }

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
    if (aml_region_offset_read(state, node, &regionOffset) == ERR)
    {
        return ERR;
    }

    aml_region_len_t regionLen;
    if (aml_region_len_read(state, node, &regionLen) == ERR)
    {
        return ERR;
    }

    aml_node_t* newNode = aml_node_add_at_name_string(&nameString, node, AML_NODE_OPREGION);
    if (newNode == NULL)
    {
        return ERR;
    }
    newNode->data.opregion.space = regionSpace;
    newNode->data.opregion.offset = regionOffset;
    newNode->data.opregion.length = regionLen;

    return 0;
}

uint64_t aml_field_flags_read(aml_state_t* state, aml_field_flags_t* out)
{
    aml_byte_data_t flags;
    if (aml_byte_data_read(state, &flags) == ERR)
    {
        return ERR;
    }

    if (flags & (1 << 7))
    {
        AML_DEBUG_INVALID_STRUCTURE("FieldFlags");
        errno = EILSEQ;
        return ERR;
    }

    aml_access_type_t accessType = flags & 0xF;
    if (accessType > AML_ACCESS_TYPE_BUFFER)
    {
        AML_DEBUG_INVALID_STRUCTURE("FieldFlags: Invalid AccessType");
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

uint64_t aml_named_field_read(aml_state_t* state, aml_node_t* node, aml_field_list_ctx_t* ctx)
{
    aml_name_seg_t* name;
    if (aml_name_seg_read(state, &name) == ERR)
    {
        return ERR;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    switch (ctx->type)
    {
    case AML_FIELD_LIST_TYPE_NORMAL:
    {
        if (ctx->normal.opregion == NULL)
        {
            AML_DEBUG_INVALID_STRUCTURE("NamedField: No OpRegion for FieldList");
            errno = EILSEQ;
            return ERR;
        }

        aml_node_t* newNode = aml_node_add(node, name->name, AML_NODE_FIELD);
        if (newNode == NULL)
        {
            return ERR;
        }
        newNode->data.field.opregion = ctx->normal.opregion;
        newNode->data.field.flags = ctx->flags;
        newNode->data.field.bitOffset = ctx->currentOffset;
        newNode->data.field.bitSize = pkgLength;
    }
    break;
    case AML_FIELD_LIST_TYPE_INDEX:
    {
        if (ctx->index.indexNode == NULL)
        {
            AML_DEBUG_INVALID_STRUCTURE("NamedField: No Index Node for IndexField");
            errno = EILSEQ;
            return ERR;
        }

        if (ctx->index.indexNode->type != AML_NODE_FIELD)
        {
            AML_DEBUG_INVALID_STRUCTURE("NamedField: Index Node is not a Field");
            errno = EILSEQ;
            return ERR;
        }

        if (ctx->index.dataNode == NULL)
        {
            AML_DEBUG_INVALID_STRUCTURE("NamedField: No Data Node for IndexField");
            errno = EILSEQ;
            return ERR;
        }

        aml_node_t* newNode = aml_node_add(node, name->name, AML_NODE_INDEX_FIELD);
        if (newNode == NULL)
        {
            return ERR;
        }
        newNode->data.indexField.indexNode = ctx->index.indexNode;
        newNode->data.indexField.dataNode = ctx->index.dataNode;
        newNode->data.indexField.flags = ctx->flags;
        newNode->data.indexField.bitOffset = ctx->currentOffset;
        newNode->data.indexField.bitSize = pkgLength;
    }
    break;
    default:
        AML_DEBUG_INVALID_STRUCTURE("NamedField: Invalid FieldList type");
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
        return ERR;
    }

    if (value.num != 0x00)
    {
        AML_DEBUG_INVALID_STRUCTURE("ReservedField: Expected 0x00");
        errno = EILSEQ;
        return ERR;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    ctx->currentOffset += pkgLength;
    return 0;
}

uint64_t aml_field_element_read(aml_state_t* state, aml_node_t* node, aml_field_list_ctx_t* ctx)
{
    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        return ERR;
    }

    if (AML_IS_LEAD_NAME_CHAR(&value))
    {
        return aml_named_field_read(state, node, ctx);
    }
    else if (value.num == 0x00)
    {
        return aml_reserved_field_read(state, ctx);
    }

    AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_field_list_read(aml_state_t* state, aml_node_t* node, aml_field_list_ctx_t* ctx, aml_address_t end)
{
    while (end > state->pos)
    {
        // End of buffer not reached => byte is not nothing => must be a FieldElement.
        if (aml_field_element_read(state, node, ctx) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_def_field_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t fieldOp;
    if (aml_value_read(state, &fieldOp) == ERR)
    {
        return ERR;
    }

    if (fieldOp.num != AML_FIELD_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&fieldOp);
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

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(state, &fieldFlags) == ERR)
    {
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_field_list_ctx_t ctx = {
        .type = AML_FIELD_LIST_TYPE_NORMAL,
        .normal.opregion = aml_node_find(&nameString, node),
        .flags = fieldFlags,
        .currentOffset = 0,
    };

    if (ctx.normal.opregion == NULL)
    {
        LOG_ERR("Field OpRegion '%s' not found\n", aml_name_string_to_string(&nameString));
        AML_DEBUG_INVALID_STRUCTURE("Field: No OpRegion for FieldList");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_field_list_read(state, node, &ctx, end) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_def_index_field_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t indexFieldOp;
    if (aml_value_read(state, &indexFieldOp) == ERR)
    {
        return ERR;
    }

    if (indexFieldOp.num != AML_INDEX_FIELD_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&indexFieldOp);
        errno = EILSEQ;
        return ERR;
    }

    aml_address_t start = state->pos;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    aml_name_string_t indexNameString;
    if (aml_name_string_read(state, &indexNameString) == ERR)
    {
        return ERR;
    }

    aml_name_string_t dataNameString;
    if (aml_name_string_read(state, &dataNameString) == ERR)
    {
        return ERR;
    }

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(state, &fieldFlags) == ERR)
    {
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_field_list_ctx_t ctx = {
        .type = AML_FIELD_LIST_TYPE_INDEX,
        .index.indexNode = aml_node_find(&indexNameString, node),
        .index.dataNode = aml_node_find(&dataNameString, node),
        .flags = fieldFlags,
        .currentOffset = 0,
    };

    if (ctx.index.indexNode == NULL)
    {
        LOG_ERR("IndexField IndexNode '%s' not found\n", aml_name_string_to_string(&indexNameString));
        AML_DEBUG_INVALID_STRUCTURE("IndexField: No IndexNode for FieldList");
        errno = EILSEQ;
        return ERR;
    }

    if (ctx.index.dataNode == NULL)
    {
        LOG_ERR("IndexField DataNode '%s' not found\n", aml_name_string_to_string(&dataNameString));
        AML_DEBUG_INVALID_STRUCTURE("IndexField: No DataNode for FieldList");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_field_list_read(state, node, &ctx, end) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_method_flags_read(aml_state_t* state, aml_method_flags_t* out)
{
    aml_byte_data_t flags;
    if (aml_byte_data_read(state, &flags) == ERR)
    {
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

uint64_t aml_def_method_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t methodOp;
    if (aml_value_read_no_ext(state, &methodOp) == ERR)
    {
        return ERR;
    }

    if (methodOp.num != AML_METHOD_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&methodOp);
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

    aml_method_flags_t methodFlags;
    if (aml_method_flags_read(state, &methodFlags) == ERR)
    {
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_node_t* newNode = aml_node_add_at_name_string(&nameString, node, AML_NODE_METHOD);
    if (newNode == NULL)
    {
        return ERR;
    }

    newNode->data.method.flags = methodFlags;
    newNode->data.method.start = state->pos;
    newNode->data.method.end = end;

    // We are only defining the method, not executing it, so we skip its body, and only parse it when it is called.
    state->pos = end;

    return 0;
}

uint64_t aml_def_device_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t deviceOp;
    if (aml_value_read(state, &deviceOp) == ERR)
    {
        return ERR;
    }

    if (deviceOp.num != AML_DEVICE_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&deviceOp);
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

    aml_node_t* newNode = aml_node_add_at_name_string(&nameString, node, AML_NODE_DEVICE);
    if (newNode == NULL)
    {
        return ERR;
    }

    return aml_term_list_read(state, newNode, end);
}

uint64_t aml_sync_flags_read(aml_state_t* state, aml_sync_level_t* out)
{
    aml_byte_data_t flags;
    if (aml_byte_data_read(state, &flags) == ERR)
    {
        return ERR;
    }

    if (flags & 0xF0)
    {
        AML_DEBUG_INVALID_STRUCTURE("SyncFlags");
        errno = EILSEQ;
        return ERR;
    }

    *out = flags & 0x0F;
    return 0;
}

uint64_t aml_def_mutex_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t mutexOp;
    if (aml_value_read(state, &mutexOp) == ERR)
    {
        return ERR;
    }

    if (mutexOp.num != AML_MUTEX_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&mutexOp);
        errno = EILSEQ;
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        return ERR;
    }

    aml_sync_level_t syncFlags;
    if (aml_sync_flags_read(state, &syncFlags) == ERR)
    {
        return ERR;
    }

    aml_node_t* newNode = aml_node_add_at_name_string(&nameString, node, AML_NODE_MUTEX);
    if (newNode == NULL)
    {
        return ERR;
    }
    mutex_init(&newNode->data.mutex.mutex);
    newNode->data.mutex.syncLevel = syncFlags;

    return 0;
}

uint64_t aml_proc_id_read(aml_state_t* state, aml_proc_id_t* out)
{
    return aml_byte_data_read(state, out);
}

uint64_t aml_pblk_addr_read(aml_state_t* state, aml_pblk_addr_t* out)
{
    return aml_dword_data_read(state, out);
}

uint64_t aml_pblk_len_read(aml_state_t* state, aml_pblk_len_t* out)
{
    return aml_byte_data_read(state, out);
}

uint64_t aml_def_processor_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t processorOp;
    if (aml_value_read(state, &processorOp) == ERR)
    {
        return ERR;
    }

    if (processorOp.num != AML_DEPRECATED_PROCESSOR_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&processorOp);
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

    aml_proc_id_t procId;
    if (aml_proc_id_read(state, &procId) == ERR)
    {
        return ERR;
    }

    aml_pblk_addr_t pblkAddr;
    if (aml_pblk_addr_read(state, &pblkAddr) == ERR)
    {
        return ERR;
    }

    aml_pblk_len_t pblkLen;
    if (aml_pblk_len_read(state, &pblkLen) == ERR)
    {
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_node_t* newNode = aml_node_add_at_name_string(&nameString, node, AML_NODE_PROCESSOR);
    if (newNode == NULL)
    {
        return ERR;
    }
    newNode->type = AML_NODE_PROCESSOR;
    newNode->data.processor.procId = procId;
    newNode->data.processor.pblkAddr = pblkAddr;
    newNode->data.processor.pblkLen = pblkLen;

    return aml_term_list_read(state, newNode, end);
}

uint64_t aml_named_obj_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_OPREGION_OP:
        return aml_def_op_region_read(state, node);
    case AML_FIELD_OP:
        return aml_def_field_read(state, node);
    case AML_METHOD_OP:
        return aml_def_method_read(state, node);
    case AML_DEVICE_OP:
        return aml_def_device_read(state, node);
    case AML_MUTEX_OP:
        return aml_def_mutex_read(state, node);
    case AML_INDEX_FIELD_OP:
        return aml_def_index_field_read(state, node);
    case AML_DEPRECATED_PROCESSOR_OP:
        return aml_def_processor_read(state, node);
    default:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    }
}

/** @} */

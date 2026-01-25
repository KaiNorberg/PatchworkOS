#include <kernel/acpi/aml/encoding/named.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/data.h>
#include <kernel/acpi/aml/encoding/name.h>
#include <kernel/acpi/aml/encoding/package_length.h>
#include <kernel/acpi/aml/encoding/term.h>
#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/acpi/tables.h>
#include <kernel/log/log.h>

uint64_t aml_bank_value_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_region_space_read(aml_term_list_ctx_t* ctx, aml_region_space_t* out)
{
    uint8_t byte;
    if (aml_byte_data_read(ctx, &byte) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return _FAIL;
    }

    if (byte > AML_REGION_PCC && byte < AML_REGION_OEM_MIN)
    {
        AML_DEBUG_ERROR(ctx, "Invalid RegionSpace: '0x%x'", byte);
        errno = EILSEQ;
        return _FAIL;
    }

    *out = (aml_region_space_t)byte;
    return 0;
}

uint64_t aml_region_offset_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_region_len_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_def_opregion_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_OPREGION_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read OpRegionOp");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_region_space_t regionSpace;
    if (aml_region_space_read(ctx, &regionSpace) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RegionSpace");
        return _FAIL;
    }

    uint64_t regionOffset;
    if (aml_region_offset_read(ctx, &regionOffset) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RegionOffset");
        return _FAIL;
    }

    uint64_t regionLen;
    if (aml_region_len_read(ctx, &regionLen) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RegionLen");
        return _FAIL;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to create object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }
    UNREF_DEFER(newObject);

    if (aml_operation_region_set(newObject, regionSpace, regionOffset, regionLen) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    return 0;
}

uint64_t aml_field_flags_read(aml_term_list_ctx_t* ctx, aml_field_flags_t* out)
{
    uint8_t flags;
    if (aml_byte_data_read(ctx, &flags) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return _FAIL;
    }

    if (flags & (1 << 7))
    {
        AML_DEBUG_ERROR(ctx, "Reserved bit 7 is set in FieldFlags '0x%x'", flags);
        errno = EILSEQ;
        return _FAIL;
    }

    aml_access_type_t accessType = flags & 0xF;
    if (accessType > AML_ACCESS_TYPE_BUFFER)
    {
        AML_DEBUG_ERROR(ctx, "Invalid AccessType in FieldFlags '0x%x'", accessType);
        errno = EILSEQ;
        return _FAIL;
    }

    *out = (aml_field_flags_t){
        .accessType = accessType,
        .lockRule = (flags >> 4) & 0x1,
        .updateRule = (flags >> 5) & 0x3,
    };

    return 0;
}

uint64_t aml_name_field_read(aml_term_list_ctx_t* ctx, aml_field_list_ctx_t* fieldCtx)
{
    aml_name_seg_t* name;
    if (aml_name_seg_read(ctx, &name) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameSeg");
        return _FAIL;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(newObject);

    switch (fieldCtx->type)
    {
    case AML_FIELD_LIST_TYPE_FIELD:
    {
        if (fieldCtx->field.opregion == NULL)
        {
            AML_DEBUG_ERROR(ctx, "opregion is null");
            errno = EILSEQ;
            return _FAIL;
        }

        if (aml_field_unit_field_set(newObject, fieldCtx->field.opregion, fieldCtx->flags, fieldCtx->currentOffset,
                pkgLength) == _FAIL)
        {
            return _FAIL;
        }
    }
    break;
    case AML_FIELD_LIST_TYPE_INDEX_FIELD:
    {
        if (fieldCtx->index.index == NULL)
        {
            AML_DEBUG_ERROR(ctx, "index is null");
            errno = EILSEQ;
            return _FAIL;
        }

        if (fieldCtx->index.index == NULL)
        {
            AML_DEBUG_ERROR(ctx, "dataObject is null");
            errno = EILSEQ;
            return _FAIL;
        }

        if (aml_field_unit_index_field_set(newObject, fieldCtx->index.index, fieldCtx->index.data, fieldCtx->flags,
                fieldCtx->currentOffset, pkgLength) == _FAIL)
        {
            return _FAIL;
        }
    }
    break;
    case AML_FIELD_LIST_TYPE_BANK_FIELD:
    {
        if (fieldCtx->bank.opregion == NULL)
        {
            AML_DEBUG_ERROR(ctx, "opregion is null");
            errno = EILSEQ;
            return _FAIL;
        }

        if (fieldCtx->bank.bank == NULL)
        {
            AML_DEBUG_ERROR(ctx, "bank is null");
            errno = EILSEQ;
            return _FAIL;
        }

        if (aml_field_unit_bank_field_set(newObject, fieldCtx->bank.opregion, fieldCtx->bank.bank,
                fieldCtx->bank.bankValue, fieldCtx->flags, fieldCtx->currentOffset, pkgLength) == _FAIL)
        {
            return _FAIL;
        }
    }
    break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid FieldList type '%d'", fieldCtx->type);
        errno = EILSEQ;
        return _FAIL;
    }

    if (aml_namespace_add_child(&ctx->state->overlay, ctx->scope, *name, newObject) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", AML_NAME_TO_STRING(*name));
        return _FAIL;
    }

    fieldCtx->currentOffset += pkgLength;
    return 0;
}

uint64_t aml_reserved_field_read(aml_term_list_ctx_t* ctx, aml_field_list_ctx_t* fieldCtx)
{
    if (aml_token_expect(ctx, 0x00) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ReservedField");
        return _FAIL;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    fieldCtx->currentOffset += pkgLength;
    return 0;
}

uint64_t aml_field_element_read(aml_term_list_ctx_t* ctx, aml_field_list_ctx_t* fieldCtx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    if (AML_IS_LEAD_NAME_CHAR(&token))
    {
        if (aml_name_field_read(ctx, fieldCtx) == _FAIL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read NamedField");
            return _FAIL;
        }
    }
    else if (token.num == 0x00)
    {
        if (aml_reserved_field_read(ctx, fieldCtx) == _FAIL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ReservedField");
            return _FAIL;
        }
    }
    else
    {
        AML_DEBUG_ERROR(ctx, "Invalid field element token '0x%x'", token.num);
        errno = ENOSYS;
        return _FAIL;
    }

    return 0;
}

uint64_t aml_field_list_read(aml_term_list_ctx_t* ctx, aml_field_list_ctx_t* fieldCtx, const uint8_t* end)
{
    while (end > ctx->current)
    {
        // End of buffer not reached => byte is not nothing => must be a FieldElement.
        if (aml_field_element_read(ctx, fieldCtx) == _FAIL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read field element");
            return _FAIL;
        }
    }

    return 0;
}

uint64_t aml_def_field_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_FIELD_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read FieldOp");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    aml_object_t* opregion = aml_name_string_read_and_resolve(ctx);
    if (opregion == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve NameString");
        return _FAIL;
    }
    UNREF_DEFER(opregion);

    if (opregion->type != AML_OPERATION_REGION)
    {
        AML_DEBUG_ERROR(ctx, "OpRegion is not of type OperationRegion");
        errno = EILSEQ;
        return _FAIL;
    }

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(ctx, &fieldFlags) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read field flags");
        return _FAIL;
    }

    const uint8_t* end = start + pkgLength;

    aml_field_list_ctx_t fieldCtx = {
        .type = AML_FIELD_LIST_TYPE_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .field.opregion = &opregion->opregion,
    };

    if (aml_field_list_read(ctx, &fieldCtx, end) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read field list");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_def_index_field_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_INDEX_FIELD_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read IndexFieldOp");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    aml_object_t* index = aml_name_string_read_and_resolve(ctx);
    if (index == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve index NameString");
        return _FAIL;
    }
    UNREF_DEFER(index);

    aml_object_t* data = aml_name_string_read_and_resolve(ctx);
    if (data == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve data NameString");
        return _FAIL;
    }
    UNREF_DEFER(data);

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(ctx, &fieldFlags) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read field flags");
        return _FAIL;
    }

    if (index->type != AML_FIELD_UNIT)
    {
        AML_DEBUG_ERROR(ctx, "Index is not of type FieldUnit");
        errno = EILSEQ;
        return _FAIL;
    }

    if (data->type != AML_FIELD_UNIT)
    {
        AML_DEBUG_ERROR(ctx, "Data is not of type FieldUnit");
        errno = EILSEQ;
        return _FAIL;
    }

    const uint8_t* end = start + pkgLength;

    aml_field_list_ctx_t fieldCtx = {.type = AML_FIELD_LIST_TYPE_INDEX_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .index.index = &index->fieldUnit,
        .index.data = &data->fieldUnit};

    if (aml_field_list_read(ctx, &fieldCtx, end) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read field list");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_def_bank_field_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_BANK_FIELD_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BankFieldOp");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* opregion = aml_name_string_read_and_resolve(ctx);
    if (opregion == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve opregion NameString");
        return _FAIL;
    }
    UNREF_DEFER(opregion);

    aml_object_t* bank = aml_name_string_read_and_resolve(ctx);
    if (bank == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve bank NameString");
        return _FAIL;
    }
    UNREF_DEFER(bank);

    uint64_t bankValue;
    if (aml_bank_value_read(ctx, &bankValue) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BankValue");
        return _FAIL;
    }

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(ctx, &fieldFlags) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read FieldFlags");
        return _FAIL;
    }

    aml_field_list_ctx_t fieldCtx = {
        .type = AML_FIELD_LIST_TYPE_BANK_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .bank.opregion = &opregion->opregion,
        .bank.bank = &bank->fieldUnit,
        .bank.bankValue = bankValue,
    };

    if (aml_field_list_read(ctx, &fieldCtx, end) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read FieldList");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_method_flags_read(aml_term_list_ctx_t* ctx, aml_method_flags_t* out)
{
    uint8_t flags;
    if (aml_byte_data_read(ctx, &flags) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return _FAIL;
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

uint64_t aml_def_method_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_METHOD_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MethodOp");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_method_flags_t methodFlags;
    if (aml_method_flags_read(ctx, &methodFlags) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MethodFlags");
        return _FAIL;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(newObject);

    if (aml_method_set(newObject, methodFlags, ctx->current, end, NULL) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    // We are only defining the method, not executing it, so we skip its body, and only parse it when it is called.
    ctx->current = end;

    return 0;
}

uint64_t aml_def_device_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_DEVICE_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DeviceOp");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* device = aml_object_new();
    if (device == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(device);

    if (aml_device_set(device) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, device) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    if (aml_term_list_read(ctx->state, device, ctx->current, end, ctx) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Device body");
        return _FAIL;
    }

    ctx->current = end;
    return 0;
}

uint64_t aml_sync_flags_read(aml_term_list_ctx_t* ctx, aml_sync_level_t* out)
{
    uint8_t flags;
    if (aml_byte_data_read(ctx, &flags) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return _FAIL;
    }

    if (flags & 0xF0)
    {
        AML_DEBUG_ERROR(ctx, "Reserved bits are set in SyncFlags '0x%x'", flags);
        errno = EILSEQ;
        return _FAIL;
    }

    *out = flags & 0x0F;
    return 0;
}

uint64_t aml_def_mutex_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_MUTEX_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MutexOp");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_sync_level_t syncFlags;
    if (aml_sync_flags_read(ctx, &syncFlags) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SyncFlags");
        return _FAIL;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(newObject);

    if (aml_mutex_set(newObject, syncFlags) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    return 0;
}

uint64_t aml_proc_id_read(aml_term_list_ctx_t* ctx, aml_proc_id_t* out)
{
    if (aml_byte_data_read(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return _FAIL;
    }
    return 0;
}

uint64_t aml_pblk_addr_read(aml_term_list_ctx_t* ctx, aml_pblk_addr_t* out)
{
    if (aml_dword_data_read(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DWordData");
        return _FAIL;
    }
    return 0;
}

uint64_t aml_pblk_len_read(aml_term_list_ctx_t* ctx, aml_pblk_len_t* out)
{
    if (aml_byte_data_read(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return _FAIL;
    }
    return 0;
}

uint64_t aml_def_processor_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_DEPRECATED_PROCESSOR_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ProcessorOp");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_proc_id_t procId;
    if (aml_proc_id_read(ctx, &procId) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read proc id");
        return _FAIL;
    }

    aml_pblk_addr_t pblkAddr;
    if (aml_pblk_addr_read(ctx, &pblkAddr) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read pblk addr");
        return _FAIL;
    }

    aml_pblk_len_t pblkLen;
    if (aml_pblk_len_read(ctx, &pblkLen) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read pblk len");
        return _FAIL;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* processor = aml_object_new();
    if (processor == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(processor);

    if (aml_processor_set(processor, procId, pblkAddr, pblkLen) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, processor) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    if (aml_term_list_read(ctx->state, processor, ctx->current, end, ctx) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Processor body");
        return _FAIL;
    }

    ctx->current = end;
    return 0;
}

aml_object_t* aml_source_buff_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* sourceBuff = aml_term_arg_read(ctx, AML_BUFFER);
    if (sourceBuff == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    return sourceBuff; // Transfer ownership
}

uint64_t aml_bit_index_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_byte_index_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_def_create_bit_field_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_CREATE_BIT_FIELD_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read CreateBitFieldOp");
        return _FAIL;
    }

    aml_object_t* sourceBuff = aml_source_buff_read(ctx);
    if (sourceBuff == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SourceBuff");
        return _FAIL;
    }
    UNREF_DEFER(sourceBuff);

    assert(sourceBuff->type == AML_BUFFER);

    uint64_t bitIndex;
    if (aml_bit_index_read(ctx, &bitIndex) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BitIndex");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(newObject);

    if (aml_buffer_field_set(newObject, sourceBuff, bitIndex, 1) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    return 0;
}

static inline uint64_t aml_def_create_field_read_helper(aml_term_list_ctx_t* ctx, uint8_t fieldWidth,
    aml_token_num_t expectedOp)
{
    if (aml_token_expect(ctx, expectedOp) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read expected op");
        return _FAIL;
    }

    aml_object_t* sourceBuff = aml_source_buff_read(ctx);
    if (sourceBuff == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SourceBuff");
        return _FAIL;
    }
    UNREF_DEFER(sourceBuff);

    assert(sourceBuff->type == AML_BUFFER);

    uint64_t byteIndex;
    if (aml_byte_index_read(ctx, &byteIndex) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteIndex");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(newObject);

    if (aml_buffer_field_set(newObject, sourceBuff, byteIndex * 8, fieldWidth) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    return 0;
}

uint64_t aml_def_create_byte_field_read(aml_term_list_ctx_t* ctx)
{
    return aml_def_create_field_read_helper(ctx, 8, AML_CREATE_BYTE_FIELD_OP);
}

uint64_t aml_def_create_word_field_read(aml_term_list_ctx_t* ctx)
{
    return aml_def_create_field_read_helper(ctx, 16, AML_CREATE_WORD_FIELD_OP);
}

uint64_t aml_def_create_dword_field_read(aml_term_list_ctx_t* ctx)
{
    return aml_def_create_field_read_helper(ctx, 32, AML_CREATE_DWORD_FIELD_OP);
}

uint64_t aml_def_create_qword_field_read(aml_term_list_ctx_t* ctx)
{
    return aml_def_create_field_read_helper(ctx, 64, AML_CREATE_QWORD_FIELD_OP);
}

uint64_t aml_def_event_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_EVENT_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read EventOp");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(newObject);

    if (aml_event_set(newObject) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    return 0;
}

uint64_t aml_def_thermal_zone_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_THERMAL_ZONE_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ThermalZoneOp");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* thermalZone = aml_object_new();
    if (thermalZone == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(thermalZone);

    if (aml_thermal_zone_set(thermalZone) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, thermalZone) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    if (aml_term_list_read(ctx->state, thermalZone, ctx->current, end, ctx) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ThermalZone body");
        return _FAIL;
    }

    ctx->current = end;
    return 0;
}

uint64_t aml_system_level_read(aml_term_list_ctx_t* ctx, aml_system_level_t* out)
{
    if (aml_byte_data_read(ctx, (uint8_t*)out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return _FAIL;
    }
    return 0;
}

uint64_t aml_resource_order_read(aml_term_list_ctx_t* ctx, aml_resource_order_t* out)
{
    if (aml_word_data_read(ctx, (uint16_t*)out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read WordData");
        return _FAIL;
    }
    return 0;
}

uint64_t aml_def_power_res_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_POWER_RES_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PowerResOp");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_system_level_t systemLevel;
    if (aml_system_level_read(ctx, &systemLevel) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SystemLevel");
        return _FAIL;
    }

    aml_resource_order_t resourceOrder;
    if (aml_resource_order_read(ctx, &resourceOrder) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ResourceOrder");
        return _FAIL;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* powerResource = aml_object_new();
    if (powerResource == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(powerResource);

    if (aml_power_resource_set(powerResource, systemLevel, resourceOrder) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, powerResource) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    if (aml_term_list_read(ctx->state, powerResource, ctx->current, end, ctx) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PowerResource body");
        return _FAIL;
    }

    ctx->current = end;
    return 0;
}

uint64_t aml_num_bits_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_def_create_field_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_CREATE_FIELD_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read CreateFieldOp");
        return _FAIL;
    }

    aml_object_t* sourceBuff = aml_source_buff_read(ctx);
    if (sourceBuff == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SourceBuff");
        return _FAIL;
    }
    UNREF_DEFER(sourceBuff);

    assert(sourceBuff->type == AML_BUFFER);

    uint64_t bitIndex;
    if (aml_bit_index_read(ctx, &bitIndex) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BitIndex");
        return _FAIL;
    }

    uint64_t numBits;
    if (aml_num_bits_read(ctx, &numBits) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NumBits");
        return _FAIL;
    }

    aml_name_stioring_t nameString;
    if (aml_name_string_read(ctx, &nameString) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return _FAIL;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return _FAIL;
    }
    UNREF_DEFER(newObject);

    if (aml_buffer_field_set(newObject, sourceBuff, bitIndex, numBits) == _FAIL ||
        aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&nameString));
        return _FAIL;
    }

    return 0;
}

uint64_t aml_def_data_region_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_DATA_REGION_OP) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DataRegionOp");
        return _FAIL;
    }

    aml_name_stioring_t regionName;
    if (aml_name_string_read(ctx, &regionName) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RegionName");
        return _FAIL;
    }

    aml_stioring_t* signature = aml_term_arg_read_string(ctx);
    if (signature == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Signature");
        return _FAIL;
    }
    UNREF_DEFER(signature);

    aml_stioring_t* oemId = aml_term_arg_read_string(ctx);
    if (oemId == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read OemId");
        return _FAIL;
    }
    UNREF_DEFER(oemId);

    aml_stioring_t* oemTableId = aml_term_arg_read_string(ctx);
    if (oemTableId == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read OemTableId");
        return _FAIL;
    }
    UNREF_DEFER(oemTableId);

    if (signature->length != SDT_SIGNATURE_LENGTH)
    {
        AML_DEBUG_ERROR(ctx, "Invalid signature length %d", signature->length);
        errno = EILSEQ;
        return _FAIL;
    }

    uint64_t index = 0;
    sdt_header_t* table = NULL;
    while (1)
    {
        table = acpi_tables_lookup(signature->content, sizeof(sdt_header_t), index++);
        if (table == NULL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to find ACPI table with signature '%s', oemId '%s' and oemTableId '%s'",
                signature->content, oemId->content, oemTableId->content);
            errno = ENOENT;
            return _FAIL;
        }

        if (oemId->length != 0)
        {
            if (oemId->length != SDT_OEM_ID_LENGTH)
            {
                AML_DEBUG_ERROR(ctx, "Invalid oemId length %d", oemId->length);
                errno = EILSEQ;
                return _FAIL;
            }

            if (strncmp((const char*)table->oemId, (const char*)oemId->content, SDT_OEM_ID_LENGTH) != 0)
            {
                continue;
            }
        }

        if (oemTableId->length != 0)
        {
            if (oemTableId->length != SDT_OEM_TABLE_ID_LENGTH)
            {
                AML_DEBUG_ERROR(ctx, "Invalid oemTableId length %d", oemTableId->length);
                errno = EILSEQ;
                return _FAIL;
            }

            if (strncmp((const char*)table->oemTableId, (const char*)oemTableId->content, SDT_OEM_TABLE_ID_LENGTH) != 0)
            {
                continue;
            }
        }

        aml_object_t* newObject = aml_object_new();
        if (newObject == NULL)
        {
            return _FAIL;
        }
        UNREF_DEFER(newObject);

        if (aml_operation_region_set(newObject, AML_REGION_SYSTEM_MEMORY, (uint64_t)table, table->length) == _FAIL ||
            aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &regionName, newObject) == _FAIL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_stioring_to_string(&regionName));
            return _FAIL;
        }

        return 0;
    }
}

uint64_t aml_named_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    uint64_t result = 0;
    switch (op.num)
    {
    case AML_OPREGION_OP:
        result = aml_def_opregion_read(ctx);
        break;
    case AML_FIELD_OP:
        result = aml_def_field_read(ctx);
        break;
    case AML_METHOD_OP:
        result = aml_def_method_read(ctx);
        break;
    case AML_DEVICE_OP:
        result = aml_def_device_read(ctx);
        break;
    case AML_MUTEX_OP:
        result = aml_def_mutex_read(ctx);
        break;
    case AML_INDEX_FIELD_OP:
        result = aml_def_index_field_read(ctx);
        break;
    case AML_BANK_FIELD_OP:
        result = aml_def_bank_field_read(ctx);
        break;
    case AML_DEPRECATED_PROCESSOR_OP:
        result = aml_def_processor_read(ctx);
        break;
    case AML_CREATE_BIT_FIELD_OP:
        result = aml_def_create_bit_field_read(ctx);
        break;
    case AML_CREATE_BYTE_FIELD_OP:
        result = aml_def_create_byte_field_read(ctx);
        break;
    case AML_CREATE_WORD_FIELD_OP:
        result = aml_def_create_word_field_read(ctx);
        break;
    case AML_CREATE_DWORD_FIELD_OP:
        result = aml_def_create_dword_field_read(ctx);
        break;
    case AML_CREATE_QWORD_FIELD_OP:
        result = aml_def_create_qword_field_read(ctx);
        break;
    case AML_EVENT_OP:
        result = aml_def_event_read(ctx);
        break;
    case AML_THERMAL_ZONE_OP:
        result = aml_def_thermal_zone_read(ctx);
        break;
    case AML_POWER_RES_OP:
        result = aml_def_power_res_read(ctx);
        break;
    case AML_CREATE_FIELD_OP:
        result = aml_def_create_field_read(ctx);
        break;
    case AML_DATA_REGION_OP:
        result = aml_def_data_region_read(ctx);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Unknown NamedObj '%s' (0x%x)", op.props->name, op.num);
        errno = ENOSYS;
        return _FAIL;
    }

    if (result == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NamedObj '%s' (0x%x)", op.props->name, op.num);
        return _FAIL;
    }

    return 0;
}

/** @} */

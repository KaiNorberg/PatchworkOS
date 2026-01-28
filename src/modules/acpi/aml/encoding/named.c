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

status_t aml_bank_value_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_region_space_read(aml_term_list_ctx_t* ctx, aml_region_space_t* out)
{
    uint8_t byte;
    status_t status = aml_byte_data_read(ctx, &byte);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }

    if (byte > AML_REGION_PCC && byte < AML_REGION_OEM_MIN)
    {
        AML_DEBUG_ERROR(ctx, "Invalid RegionSpace: '0x%x'", byte);
        return ERR(ACPI, ILSEQ);
    }

    *out = (aml_region_space_t)byte;
    return OK;
}

status_t aml_region_offset_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_region_len_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_def_opregion_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_OPREGION_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read OpRegionOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_name_string_t nameString;
    status_t status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_region_space_t regionSpace;
    status = aml_region_space_read(ctx, &regionSpace);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RegionSpace");
        return status;
    }

    uint64_t regionOffset;
    status = aml_region_offset_read(ctx, &regionOffset);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RegionOffset");
        return status;
    }

    uint64_t regionLen;
    status = aml_region_len_read(ctx, &regionLen);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RegionLen");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to create object '%s'", aml_name_string_to_string(&nameString));
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_operation_region_set(newObject, regionSpace, regionOffset, regionLen);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    return OK;
}

status_t aml_field_flags_read(aml_term_list_ctx_t* ctx, aml_field_flags_t* out)
{
    uint8_t flags;
    status_t status = aml_byte_data_read(ctx, &flags);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }

    if (flags & (1 << 7))
    {
        AML_DEBUG_ERROR(ctx, "Reserved bit 7 is set in FieldFlags '0x%x'", flags);
        return ERR(ACPI, ILSEQ);
    }

    aml_access_type_t accessType = flags & 0xF;
    if (accessType > AML_ACCESS_TYPE_BUFFER)
    {
        AML_DEBUG_ERROR(ctx, "Invalid AccessType in FieldFlags '0x%x'", accessType);
        return ERR(ACPI, ILSEQ);
    }

    *out = (aml_field_flags_t){
        .accessType = accessType,
        .lockRule = (flags >> 4) & 0x1,
        .updateRule = (flags >> 5) & 0x3,
    };

    return OK;
}

status_t aml_name_field_read(aml_term_list_ctx_t* ctx, aml_field_list_ctx_t* fieldCtx)
{
    aml_name_seg_t* name;
    status_t status = aml_name_seg_read(ctx, &name);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameSeg");
        return status;
    }

    aml_pkg_length_t pkgLength;
    status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    switch (fieldCtx->type)
    {
    case AML_FIELD_LIST_TYPE_FIELD:
    {
        if (fieldCtx->field.opregion == NULL)
        {
            AML_DEBUG_ERROR(ctx, "opregion is null");
            return ERR(ACPI, ILSEQ);
        }

        status = aml_field_unit_field_set(newObject, fieldCtx->field.opregion, fieldCtx->flags, fieldCtx->currentOffset,
            pkgLength);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    break;
    case AML_FIELD_LIST_TYPE_INDEX_FIELD:
    {
        if (fieldCtx->index.index == NULL)
        {
            AML_DEBUG_ERROR(ctx, "index is null");
            return ERR(ACPI, ILSEQ);
        }

        if (fieldCtx->index.data == NULL)
        {
            AML_DEBUG_ERROR(ctx, "dataObject is null");
            return ERR(ACPI, ILSEQ);
        }

        status = aml_field_unit_index_field_set(newObject, fieldCtx->index.index, fieldCtx->index.data, fieldCtx->flags,
            fieldCtx->currentOffset, pkgLength);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    break;
    case AML_FIELD_LIST_TYPE_BANK_FIELD:
    {
        if (fieldCtx->bank.opregion == NULL)
        {
            AML_DEBUG_ERROR(ctx, "opregion is null");
            return ERR(ACPI, ILSEQ);
        }

        if (fieldCtx->bank.bank == NULL)
        {
            AML_DEBUG_ERROR(ctx, "bank is null");
            return ERR(ACPI, ILSEQ);
        }

        status = aml_field_unit_bank_field_set(newObject, fieldCtx->bank.opregion, fieldCtx->bank.bank,
            fieldCtx->bank.bankValue, fieldCtx->flags, fieldCtx->currentOffset, pkgLength);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid FieldList type '%d'", fieldCtx->type);
        return ERR(ACPI, ILSEQ);
    }

    status = aml_namespace_add_child(&ctx->state->overlay, ctx->scope, *name, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", AML_NAME_TO_STRING(*name));
        return status;
    }

    fieldCtx->currentOffset += pkgLength;
    return OK;
}

status_t aml_reserved_field_read(aml_term_list_ctx_t* ctx, aml_field_list_ctx_t* fieldCtx)
{
    if (!aml_token_expect(ctx, 0x00))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ReservedField");
        return ERR(ACPI, ILSEQ);
    }

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    fieldCtx->currentOffset += pkgLength;
    return OK;
}

status_t aml_field_element_read(aml_term_list_ctx_t* ctx, aml_field_list_ctx_t* fieldCtx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    if (AML_IS_LEAD_NAME_CHAR(&token))
    {
        status_t status = aml_name_field_read(ctx, fieldCtx);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read NamedField");
            return status;
        }
    }
    else if (token.num == 0x00)
    {
        status_t status = aml_reserved_field_read(ctx, fieldCtx);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ReservedField");
            return status;
        }
    }
    else
    {
        AML_DEBUG_ERROR(ctx, "Invalid field element token '0x%x'", token.num);
        return ERR(ACPI, ILSEQ);
    }

    return OK;
}

status_t aml_field_list_read(aml_term_list_ctx_t* ctx, aml_field_list_ctx_t* fieldCtx, const uint8_t* end)
{
    while (end > ctx->current)
    {
        // End of buffer not reached => byte is not nothing => must be a FieldElement.
        status_t status = aml_field_element_read(ctx, fieldCtx);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read field element");
            return status;
        }
    }

    return OK;
}

status_t aml_def_field_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_FIELD_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read FieldOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_object_t* opregion;
    status = aml_name_string_read_and_resolve(ctx, &opregion);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve NameString");
        return status;
    }
    UNREF_DEFER(opregion);

    if (opregion->type != AML_OPERATION_REGION)
    {
        AML_DEBUG_ERROR(ctx, "OpRegion is not of type OperationRegion");
        return ERR(ACPI, ILSEQ);
    }

    aml_field_flags_t fieldFlags;
    status = aml_field_flags_read(ctx, &fieldFlags);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read field flags");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    aml_field_list_ctx_t fieldCtx = {
        .type = AML_FIELD_LIST_TYPE_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .field.opregion = &opregion->opregion,
    };

    status = aml_field_list_read(ctx, &fieldCtx, end);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read field list");
        return status;
    }

    return OK;
}

status_t aml_def_index_field_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_INDEX_FIELD_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read IndexFieldOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_object_t* index;
    status = aml_name_string_read_and_resolve(ctx, &index);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve index NameString");
        return status;
    }
    UNREF_DEFER(index);

    aml_object_t* data;
    status = aml_name_string_read_and_resolve(ctx, &data);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve data NameString");
        return status;
    }
    UNREF_DEFER(data);

    aml_field_flags_t fieldFlags;
    status = aml_field_flags_read(ctx, &fieldFlags);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read field flags");
        return status;
    }

    if (index->type != AML_FIELD_UNIT)
    {
        AML_DEBUG_ERROR(ctx, "Index is not of type FieldUnit");
        return ERR(ACPI, ILSEQ);
    }

    if (data->type != AML_FIELD_UNIT)
    {
        AML_DEBUG_ERROR(ctx, "Data is not of type FieldUnit");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* end = start + pkgLength;

    aml_field_list_ctx_t fieldCtx = {.type = AML_FIELD_LIST_TYPE_INDEX_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .index.index = &index->fieldUnit,
        .index.data = &data->fieldUnit};

    status = aml_field_list_read(ctx, &fieldCtx, end);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read field list");
        return status;
    }

    return OK;
}

status_t aml_def_bank_field_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_BANK_FIELD_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BankFieldOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* opregion;
    status = aml_name_string_read_and_resolve(ctx, &opregion);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve opregion NameString");
        return status;
    }
    UNREF_DEFER(opregion);

    aml_object_t* bank;
    status = aml_name_string_read_and_resolve(ctx, &bank);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve bank NameString");
        return status;
    }
    UNREF_DEFER(bank);

    uint64_t bankValue;
    status = aml_bank_value_read(ctx, &bankValue);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BankValue");
        return status;
    }

    aml_field_flags_t fieldFlags;
    status = aml_field_flags_read(ctx, &fieldFlags);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read FieldFlags");
        return status;
    }

    aml_field_list_ctx_t fieldCtx = {
        .type = AML_FIELD_LIST_TYPE_BANK_FIELD,
        .flags = fieldFlags,
        .currentOffset = 0,
        .bank.opregion = &opregion->opregion,
        .bank.bank = &bank->fieldUnit,
        .bank.bankValue = bankValue,
    };

    status = aml_field_list_read(ctx, &fieldCtx, end);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read FieldList");
        return status;
    }

    return OK;
}

status_t aml_method_flags_read(aml_term_list_ctx_t* ctx, aml_method_flags_t* out)
{
    uint8_t flags;
    status_t status = aml_byte_data_read(ctx, &flags);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }

    uint8_t argCount = flags & 0x7;
    bool isSerialized = (flags >> 3) & 0x1;
    uint8_t syncLevel = (flags >> 4) & 0xF;

    *out = (aml_method_flags_t){
        .argCount = argCount,
        .isSerialized = isSerialized,
        .syncLevel = syncLevel,
    };

    return OK;
}

status_t aml_def_method_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_METHOD_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MethodOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_method_flags_t methodFlags;
    status = aml_method_flags_read(ctx, &methodFlags);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MethodFlags");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_method_set(newObject, methodFlags, ctx->current, end, NULL);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    // We are only defining the method, not executing it, so we skip its body, and only parse it when it is called.
    ctx->current = end;

    return OK;
}

status_t aml_def_device_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_DEVICE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DeviceOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* device = aml_object_new();
    if (device == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(device);

    status = aml_device_set(device);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, device);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    status = aml_term_list_read(ctx->state, device, ctx->current, end, ctx);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Device body");
        return status;
    }

    ctx->current = end;
    return OK;
}

status_t aml_sync_flags_read(aml_term_list_ctx_t* ctx, aml_sync_level_t* out)
{
    uint8_t flags;
    status_t status = aml_byte_data_read(ctx, &flags);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }

    if (flags & 0xF0)
    {
        AML_DEBUG_ERROR(ctx, "Reserved bits are set in SyncFlags '0x%x'", flags);
        return ERR(ACPI, ILSEQ);
    }

    *out = flags & 0x0F;
    return OK;
}

status_t aml_def_mutex_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_MUTEX_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MutexOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_name_string_t nameString;
    status_t status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_sync_level_t syncFlags;
    status = aml_sync_flags_read(ctx, &syncFlags);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SyncFlags");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_mutex_set(newObject, syncFlags);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    return OK;
}

status_t aml_proc_id_read(aml_term_list_ctx_t* ctx, aml_proc_id_t* out)
{
    status_t status = aml_byte_data_read(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }
    return OK;
}

status_t aml_pblk_addr_read(aml_term_list_ctx_t* ctx, aml_pblk_addr_t* out)
{
    status_t status = aml_dword_data_read(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DWordData");
        return status;
    }
    return OK;
}

status_t aml_pblk_len_read(aml_term_list_ctx_t* ctx, aml_pblk_len_t* out)
{
    status_t status = aml_byte_data_read(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }
    return OK;
}

status_t aml_def_processor_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_DEPRECATED_PROCESSOR_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ProcessorOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_proc_id_t procId;
    status = aml_proc_id_read(ctx, &procId);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read proc id");
        return status;
    }

    aml_pblk_addr_t pblkAddr;
    status = aml_pblk_addr_read(ctx, &pblkAddr);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read pblk addr");
        return status;
    }

    aml_pblk_len_t pblkLen;
    status = aml_pblk_len_read(ctx, &pblkLen);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read pblk len");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* processor = aml_object_new();
    if (processor == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(processor);

    status = aml_processor_set(processor, procId, pblkAddr, pblkLen);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, processor);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    status = aml_term_list_read(ctx->state, processor, ctx->current, end, ctx);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Processor body");
        return status;
    }

    ctx->current = end;
    return OK;
}

status_t aml_source_buff_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status = aml_term_arg_read(ctx, AML_BUFFER, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_bit_index_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_byte_index_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_def_create_bit_field_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_CREATE_BIT_FIELD_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read CreateBitFieldOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* sourceBuff;
    status_t status = aml_source_buff_read(ctx, &sourceBuff);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SourceBuff");
        return status;
    }
    UNREF_DEFER(sourceBuff);

    assert(sourceBuff->type == AML_BUFFER);

    uint64_t bitIndex;
    status = aml_bit_index_read(ctx, &bitIndex);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BitIndex");
        return status;
    }

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_buffer_field_set(newObject, sourceBuff, bitIndex, 1);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    return OK;
}

static inline status_t aml_def_create_field_read_helper(aml_term_list_ctx_t* ctx, uint8_t fieldWidth,
    aml_token_num_t expectedOp)
{
    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read expected op");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* sourceBuff;
    status_t status = aml_source_buff_read(ctx, &sourceBuff);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SourceBuff");
        return status;
    }
    UNREF_DEFER(sourceBuff);

    assert(sourceBuff->type == AML_BUFFER);

    uint64_t byteIndex;
    status = aml_byte_index_read(ctx, &byteIndex);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteIndex");
        return status;
    }

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_buffer_field_set(newObject, sourceBuff, byteIndex * 8, fieldWidth);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    return OK;
}

status_t aml_def_create_byte_field_read(aml_term_list_ctx_t* ctx)
{
    return aml_def_create_field_read_helper(ctx, 8, AML_CREATE_BYTE_FIELD_OP);
}

status_t aml_def_create_word_field_read(aml_term_list_ctx_t* ctx)
{
    return aml_def_create_field_read_helper(ctx, 16, AML_CREATE_WORD_FIELD_OP);
}

status_t aml_def_create_dword_field_read(aml_term_list_ctx_t* ctx)
{
    return aml_def_create_field_read_helper(ctx, 32, AML_CREATE_DWORD_FIELD_OP);
}

status_t aml_def_create_qword_field_read(aml_term_list_ctx_t* ctx)
{
    return aml_def_create_field_read_helper(ctx, 64, AML_CREATE_QWORD_FIELD_OP);
}

status_t aml_def_event_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_EVENT_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read EventOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_name_string_t nameString;
    status_t status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_event_set(newObject);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    return OK;
}

status_t aml_def_thermal_zone_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_THERMAL_ZONE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ThermalZoneOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* thermalZone = aml_object_new();
    if (thermalZone == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(thermalZone);

    status = aml_thermal_zone_set(thermalZone);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, thermalZone);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    status = aml_term_list_read(ctx->state, thermalZone, ctx->current, end, ctx);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ThermalZone body");
        return status;
    }

    ctx->current = end;
    return OK;
}

status_t aml_system_level_read(aml_term_list_ctx_t* ctx, aml_system_level_t* out)
{
    status_t status = aml_byte_data_read(ctx, (uint8_t*)out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }
    return OK;
}

status_t aml_resource_order_read(aml_term_list_ctx_t* ctx, aml_resource_order_t* out)
{
    status_t status = aml_word_data_read(ctx, (uint16_t*)out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read WordData");
        return status;
    }
    return OK;
}

status_t aml_def_power_res_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_POWER_RES_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PowerResOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_system_level_t systemLevel;
    status = aml_system_level_read(ctx, &systemLevel);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SystemLevel");
        return status;
    }

    aml_resource_order_t resourceOrder;
    status = aml_resource_order_read(ctx, &resourceOrder);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ResourceOrder");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    aml_object_t* powerResource = aml_object_new();
    if (powerResource == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(powerResource);

    status = aml_power_resource_set(powerResource, systemLevel, resourceOrder);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, powerResource);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    status = aml_term_list_read(ctx->state, powerResource, ctx->current, end, ctx);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PowerResource body");
        return status;
    }

    ctx->current = end;
    return OK;
}

status_t aml_num_bits_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_def_create_field_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_CREATE_FIELD_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read CreateFieldOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* sourceBuff;
    status_t status = aml_source_buff_read(ctx, &sourceBuff);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SourceBuff");
        return status;
    }
    UNREF_DEFER(sourceBuff);

    assert(sourceBuff->type == AML_BUFFER);

    uint64_t bitIndex;
    status = aml_bit_index_read(ctx, &bitIndex);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BitIndex");
        return status;
    }

    uint64_t numBits;
    status = aml_num_bits_read(ctx, &numBits);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NumBits");
        return status;
    }

    aml_name_string_t nameString;
    status = aml_name_string_read(ctx, &nameString);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_object_t* newObject = aml_object_new();
    if (newObject == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(newObject);

    status = aml_buffer_field_set(newObject, sourceBuff, bitIndex, numBits);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &nameString, newObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&nameString));
        return status;
    }

    return OK;
}

status_t aml_def_data_region_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_DATA_REGION_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DataRegionOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_name_string_t regionName;
    status_t status = aml_name_string_read(ctx, &regionName);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RegionName");
        return status;
    }

    aml_string_t* signature;
    status = aml_term_arg_read_string(ctx, &signature);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Signature");
        return status;
    }
    UNREF_DEFER(signature);

    aml_string_t* oemId;
    status = aml_term_arg_read_string(ctx, &oemId);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read OemId");
        return status;
    }
    UNREF_DEFER(oemId);

    aml_string_t* oemTableId;
    status = aml_term_arg_read_string(ctx, &oemTableId);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read OemTableId");
        return status;
    }
    UNREF_DEFER(oemTableId);

    if (signature->length != SDT_SIGNATURE_LENGTH)
    {
        AML_DEBUG_ERROR(ctx, "Invalid signature length %d", signature->length);
        return ERR(ACPI, ILSEQ);
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
            return ERR(ACPI, NOENT);
        }

        if (oemId->length != 0)
        {
            if (oemId->length != SDT_OEM_ID_LENGTH)
            {
                AML_DEBUG_ERROR(ctx, "Invalid oemId length %d", oemId->length);
                return ERR(ACPI, ILSEQ);
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
                return ERR(ACPI, ILSEQ);
            }

            if (strncmp((const char*)table->oemTableId, (const char*)oemTableId->content, SDT_OEM_TABLE_ID_LENGTH) != 0)
            {
                continue;
            }
        }

        aml_object_t* newObject = aml_object_new();
        if (newObject == NULL)
        {
            return ERR(ACPI, NOMEM);
        }
        UNREF_DEFER(newObject);

        status = aml_operation_region_set(newObject, AML_REGION_SYSTEM_MEMORY, (uint64_t)table, table->length);
        if (IS_ERR(status))
        {
            return status;
        }

        status = aml_namespace_add_by_name_string(&ctx->state->overlay, ctx->scope, &regionName, newObject);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to add object '%s'", aml_name_string_to_string(&regionName));
            return status;
        }

        return OK;
    }
}

status_t aml_named_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    status_t result = OK;
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
        return ERR(ACPI, ILSEQ);
    }

    if (IS_ERR(result))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NamedObj '%s' (0x%x)", op.props->name, op.num);
        return result;
    }

    return OK;
}

/** @} */

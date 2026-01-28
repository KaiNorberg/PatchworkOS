#include <kernel/acpi/aml/encoding/name.h>

#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/data.h>
#include <kernel/acpi/aml/encoding/debug.h>
#include <kernel/acpi/aml/encoding/term.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/log/log.h>

#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

status_t aml_seg_count_read(aml_term_list_ctx_t* ctx, uint8_t* out)
{
    status_t status = aml_byte_data_read(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }
    return OK;
}

status_t aml_name_seg_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** out)
{
    const uint8_t* start = ctx->current;

    aml_token_t leadnamechar;
    aml_token_read(ctx, &leadnamechar);

    if (!AML_IS_LEAD_NAME_CHAR(&leadnamechar))
    {
        AML_DEBUG_ERROR(ctx, "Invalid LeadNameChar 0x%04x in NameSeg", leadnamechar.num);
        return ERR(ACPI, ILSEQ);
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        aml_token_t namechar;
        aml_token_read(ctx, &namechar);

        if (!AML_IS_NAME_CHAR(&namechar))
        {
            AML_DEBUG_ERROR(ctx, "Invalid char 0x%04x in NameSeg", namechar.num);
            return ERR(ACPI, ILSEQ);
        }
    }

    *out = (aml_name_seg_t*)start;
    return OK;
}

status_t aml_dual_name_path_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** out)
{
    if (!aml_token_expect(ctx, AML_DUAL_NAME_PREFIX))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DualNamePrefix");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    // We just read the two NameSegs to verify they are valid.
    aml_name_seg_t* temp;
    status_t status = aml_name_seg_read(ctx, &temp);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameSeg");
        return status;
    }
    status = aml_name_seg_read(ctx, &temp);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameSeg");
        return status;
    }

    *out = (aml_name_seg_t*)start;
    return OK;
}

status_t aml_multi_name_path_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** outSegments, uint64_t* outSegCount)
{
    if (!aml_token_expect(ctx, AML_MULTI_NAME_PREFIX))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MultiNamePrefix");
        return ERR(ACPI, ILSEQ);
    }

    uint8_t segCount;
    status_t status = aml_seg_count_read(ctx, &segCount);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SegCount");
        return status;
    }

    const uint8_t* start = ctx->current;

    // We just read the NameSegs to verify they are valid.
    aml_name_seg_t* temp;
    for (size_t i = 0; i < segCount; i++)
    {
        status = aml_name_seg_read(ctx, &temp);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read NameSeg");
            return status;
        }
    }

    *outSegments = (aml_name_seg_t*)start;
    *outSegCount = segCount;
    return OK;
}

status_t aml_null_name_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_NULL_NAME))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NullName");
        return ERR(ACPI, ILSEQ);
    }

    return OK;
}

status_t aml_name_path_read(aml_term_list_ctx_t* ctx, aml_name_path_t* out)
{
    aml_token_t firstToken;
    aml_token_peek(ctx, &firstToken);

    if (AML_IS_LEAD_NAME_CHAR(&firstToken))
    {
        out->segmentCount = 1;
        return aml_name_seg_read(ctx, &out->segments);
    }
    else if (firstToken.num == AML_DUAL_NAME_PREFIX)
    {
        out->segmentCount = 2;
        return aml_dual_name_path_read(ctx, &out->segments);
    }
    else if (firstToken.num == AML_MULTI_NAME_PREFIX)
    {
        return aml_multi_name_path_read(ctx, &out->segments, &out->segmentCount);
    }
    else if (firstToken.num == AML_NULL_NAME)
    {
        out->segmentCount = 0;
        out->segments = NULL;
        return aml_null_name_read(ctx);
    }
    else
    {
        AML_DEBUG_ERROR(ctx, "Invalid name that starts with 0x%x", firstToken.num);
        return ERR(ACPI, ILSEQ);
    }

    return OK;
}

status_t aml_prefix_path_read(aml_term_list_ctx_t* ctx, aml_prefix_path_t* out)
{
    out->depth = 0;
    while (true)
    {
        aml_token_t chr;
        aml_token_peek(ctx, &chr);

        if (chr.num != AML_PARENT_PREFIX_CHAR)
        {
            return OK;
        }

        ctx->current += chr.length;
        out->depth++;
    }
}

status_t aml_root_char_read(aml_term_list_ctx_t* ctx, aml_root_char_t* out)
{
    if (!aml_token_expect(ctx, AML_ROOT_CHAR))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RootChar");
        return ERR(ACPI, ILSEQ);
    }

    out->present = true;
    return OK;
}

status_t aml_name_string_read(aml_term_list_ctx_t* ctx, aml_name_string_t* out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    aml_name_string_t nameString = {0};
    status_t status = OK;
    // Starts with either rootchar or prefixpath.
    switch (token.num)
    {
    case AML_ROOT_CHAR:
        status = aml_root_char_read(ctx, &nameString.rootChar);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read root char");
            return status;
        }
        break;
    case AML_PARENT_PREFIX_CHAR:
        status = aml_prefix_path_read(ctx, &nameString.prefixPath);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read prefix path");
            return status;
        }
        break;
    default:
        // Is a empty prefixpath.
        break;
    }

    status = aml_name_path_read(ctx, &nameString.namePath);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read name path");
        return status;
    }

    *out = nameString;
    return OK;
}

status_t aml_name_string_read_and_resolve(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_name_string_t nameStringLocal;
    status_t status = aml_name_string_read(ctx, &nameStringLocal);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return status;
    }

    aml_object_t* obj = aml_namespace_find_by_name_string(&ctx->state->overlay, ctx->scope, &nameStringLocal);
    if (obj == NULL)
    {
        obj = aml_object_new();
        if (obj == NULL)
        {
            return ERR(ACPI, NOMEM);
        }

        status = aml_integer_set(obj, 0);
        if (IS_ERR(status))
        {
            UNREF(obj);
            return status;
        }

        *out = obj; // Transfer ownership
        return OK;
    }

    if (obj->type == AML_UNINITIALIZED)
    {
        UNREF(obj);
        return ERR(ACPI, NOENT);
    }

    *out = obj; // Transfer ownership
    return OK;
}

status_t aml_simple_name_read_and_resolve(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    status_t status = OK;
    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_NAME:
        status = aml_name_string_read_and_resolve(ctx, out);
        break;
    case AML_TOKEN_TYPE_ARG:
        status = aml_arg_obj_read(ctx, out);
        break;
    case AML_TOKEN_TYPE_LOCAL:
        status = aml_local_obj_read(ctx, out);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid token type '%s'", aml_token_type_to_string(token.props->type));
        return ERR(ACPI, ILSEQ);
    }

    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read '%s'", token.props->name);
        return status;
    }

    return OK; // Transfer ownership
}

status_t aml_super_name_read_and_resolve(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    status_t status = OK;
    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_NAME:
    case AML_TOKEN_TYPE_ARG:
    case AML_TOKEN_TYPE_LOCAL:
        status = aml_simple_name_read_and_resolve(ctx, out);
        break;
    case AML_TOKEN_TYPE_DEBUG:
        status = aml_debug_obj_read(ctx, out);
        break;
    case AML_TOKEN_TYPE_EXPRESSION:
        status = aml_reference_type_opcode_read(ctx, out);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid token type '%s'", aml_token_type_to_string(token.props->type));
        return ERR(ACPI, ILSEQ);
    }

    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read '%s'", token.props->name);
        return status;
    }

    return OK; // Transfer ownership
}

status_t aml_target_read_and_resolve(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    if (token.num == AML_NULL_NAME)
    {
        *out = NULL;
        status_t status = aml_null_name_read(ctx);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read null name");
            return status;
        }
    }
    else
    {
        status_t status = aml_super_name_read_and_resolve(ctx, out); // Transfer ownership
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
            return status;
        }
    }

    return OK;
}

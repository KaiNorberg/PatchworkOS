#include <kernel/acpi/aml/encoding/name.h>

#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/data.h>
#include <kernel/acpi/aml/encoding/debug.h>
#include <kernel/acpi/aml/encoding/term.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/log/log.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

uint64_t aml_seg_count_read(aml_term_list_ctx_t* ctx, uint8_t* out)
{
    if (aml_byte_data_read(ctx, out) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return _FAIL;
    }
    return 0;
}

uint64_t aml_name_seg_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** out)
{
    const uint8_t* start = ctx->current;

    aml_token_t leadnamechar;
    aml_token_read(ctx, &leadnamechar);

    if (!AML_IS_LEAD_NAME_CHAR(&leadnamechar))
    {
        AML_DEBUG_ERROR(ctx, "Invalid LeadNameChar 0x%04x in NameSeg", leadnamechar.num);
        errno = EILSEQ;
        return _FAIL;
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        aml_token_t namechar;
        aml_token_read(ctx, &namechar);

        if (!AML_IS_NAME_CHAR(&namechar))
        {
            AML_DEBUG_ERROR(ctx, "Invalid char 0x%04x in NameSeg", namechar.num);
            errno = EILSEQ;
            return _FAIL;
        }
    }

    *out = (aml_name_seg_t*)start;
    return 0;
}

uint64_t aml_dual_name_path_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** out)
{
    if (aml_token_expect(ctx, AML_DUAL_NAME_PREFIX) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DualNamePrefix");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    // We just read the two NameSegs to verify they are valid.
    aml_name_seg_t* temp;
    if (aml_name_seg_read(ctx, &temp) == _FAIL || aml_name_seg_read(ctx, &temp) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameSeg");
        return _FAIL;
    }

    *out = (aml_name_seg_t*)start;
    return 0;
}

uint64_t aml_multi_name_path_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** outSegments, uint64_t* outSegCount)
{
    if (aml_token_expect(ctx, AML_MULTI_NAME_PREFIX) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MultiNamePrefix");
        return _FAIL;
    }

    uint8_t segCount;
    if (aml_seg_count_read(ctx, &segCount) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SegCount");
        return _FAIL;
    }

    const uint8_t* start = ctx->current;

    // We just read the NameSegs to verify they are valid.
    aml_name_seg_t* temp;
    for (size_t i = 0; i < segCount; i++)
    {
        if (aml_name_seg_read(ctx, &temp) == _FAIL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read NameSeg");
            return _FAIL;
        }
    }

    *outSegments = (aml_name_seg_t*)start;
    *outSegCount = segCount;
    return 0;
}

uint64_t aml_null_name_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_NULL_NAME) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NullName");
        return _FAIL;
    }

    return 0;
}

uint64_t aml_name_path_read(aml_term_list_ctx_t* ctx, aml_name_path_t* out)
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
        errno = EILSEQ;
        return _FAIL;
    }

    return 0;
}

uint64_t aml_prefix_path_read(aml_term_list_ctx_t* ctx, aml_prefix_path_t* out)
{
    out->depth = 0;
    while (true)
    {
        aml_token_t chr;
        aml_token_peek(ctx, &chr);

        if (chr.num != AML_PARENT_PREFIX_CHAR)
        {
            return 0;
        }

        ctx->current += chr.length;
        out->depth++;
    }
}

uint64_t aml_root_char_read(aml_term_list_ctx_t* ctx, aml_root_char_t* out)
{
    if (aml_token_expect(ctx, AML_ROOT_CHAR) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read RootChar");
        return _FAIL;
    }

    out->present = true;
    return 0;
}

uint64_t aml_name_string_read(aml_term_list_ctx_t* ctx, aml_name_stioring_t* out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    aml_name_stioring_t nameString = {0};
    // Starts with either rootchar or prefixpath.
    switch (token.num)
    {
    case AML_ROOT_CHAR:
        if (aml_root_char_read(ctx, &nameString.rootChar) == _FAIL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read root char");
            return _FAIL;
        }
        break;
    case AML_PARENT_PREFIX_CHAR:
        if (aml_prefix_path_read(ctx, &nameString.prefixPath) == _FAIL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read prefix path");
            return _FAIL;
        }
        break;
    default:
        // Is a empty prefixpath.
        break;
    }

    if (aml_name_path_read(ctx, &nameString.namePath) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read name path");
        return _FAIL;
    }

    *out = nameString;
    return 0;
}

aml_object_t* aml_name_string_read_and_resolve(aml_term_list_ctx_t* ctx)
{
    aml_name_stioring_t nameStringLocal;
    if (aml_name_string_read(ctx, &nameStringLocal) == _FAIL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NameString");
        return NULL;
    }

    aml_object_t* out = aml_namespace_find_by_name_string(&ctx->state->overlay, ctx->scope, &nameStringLocal);
    if (out == NULL)
    {
        out = aml_object_new();
        if (out == NULL)
        {
            return NULL;
        }

        if (aml_integer_set(out, 0) == _FAIL)
        {
            UNREF(out);
            return NULL;
        }

        return out; // Transfer ownership
    }

    if (out->type == AML_UNINITIALIZED)
    {
        errno = ENOENT;
        UNREF(out);
        return NULL;
    }

    return out; // Transfer ownership
}

aml_object_t* aml_simple_name_read_and_resolve(aml_term_list_ctx_t* ctx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    aml_object_t* out = NULL;
    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_NAME:
        out = aml_name_string_read_and_resolve(ctx);
        break;
    case AML_TOKEN_TYPE_ARG:
        out = aml_arg_obj_read(ctx);
        break;
    case AML_TOKEN_TYPE_LOCAL:
        out = aml_local_obj_read(ctx);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid token type '%s'", aml_token_type_to_string(token.props->type));
        errno = EILSEQ;
        return NULL;
    }

    if (out == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read '%s'", token.props->name);
        return NULL;
    }

    return out; // Transfer ownership
}

aml_object_t* aml_super_name_read_and_resolve(aml_term_list_ctx_t* ctx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    aml_object_t* out = NULL;
    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_NAME:
    case AML_TOKEN_TYPE_ARG:
    case AML_TOKEN_TYPE_LOCAL:
        out = aml_simple_name_read_and_resolve(ctx);
        break;
    case AML_TOKEN_TYPE_DEBUG:
        out = aml_debug_obj_read(ctx);
        break;
    case AML_TOKEN_TYPE_EXPRESSION:
        out = aml_reference_type_opcode_read(ctx);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid token type '%s'", aml_token_type_to_string(token.props->type));
        errno = EILSEQ;
        return NULL;
    }

    if (out == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read '%s'", token.props->name);
        return NULL;
    }

    return out; // Transfer ownership
}

uint64_t aml_target_read_and_resolve(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    if (token.num == AML_NULL_NAME)
    {
        *out = NULL;
        if (aml_null_name_read(ctx) == _FAIL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read null name");
            return _FAIL;
        }
    }
    else
    {
        *out = aml_super_name_read_and_resolve(ctx); // Transfer ownership
        if (*out == NULL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
            return _FAIL;
        }
    }

    return 0;
}

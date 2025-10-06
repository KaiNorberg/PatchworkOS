#include "name.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_token.h"
#include "data.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

uint64_t aml_seg_count_read(aml_state_t* state, uint8_t* out)
{
    if (aml_byte_data_read(state, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ByteData");
        return ERR;
    }
    return 0;
}

uint64_t aml_name_seg_read(aml_state_t* state, aml_name_seg_t** out)
{
    const uint8_t* start = state->current;

    aml_token_t leadnamechar;
    if (aml_token_read(state, &leadnamechar) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read LeadNameChar");
        return ERR;
    }

    if (!AML_IS_LEAD_NAME_CHAR(&leadnamechar))
    {
        AML_DEBUG_ERROR(state, "Invalid LeadNameChar 0x%04x in NameSeg", leadnamechar.num);
        errno = EILSEQ;
        return ERR;
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        aml_token_t namechar;
        if (aml_token_read(state, &namechar) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read NameChar");
            return ERR;
        }

        if (!AML_IS_NAME_CHAR(&namechar))
        {
            AML_DEBUG_ERROR(state, "Invalid char 0x%04x in NameSeg", namechar.num);
            errno = EILSEQ;
            return ERR;
        }
    }

    *out = (aml_name_seg_t*)start;
    return 0;
}

uint64_t aml_dual_name_path_read(aml_state_t* state, aml_name_seg_t** out)
{
    if (aml_token_expect(state, AML_DUAL_NAME_PREFIX) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DualNamePrefix");
        return ERR;
    }

    const uint8_t* start = state->current;

    // We just read the two NameSegs to verify they are valid.
    aml_name_seg_t* temp;
    if (aml_name_seg_read(state, &temp) == ERR || aml_name_seg_read(state, &temp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameSeg");
        return ERR;
    }

    *out = (aml_name_seg_t*)start;
    return 0;
}

uint64_t aml_multi_name_path_read(aml_state_t* state, aml_name_seg_t** outSegments, uint64_t* outSegCount)
{
    if (aml_token_expect(state, AML_MULTI_NAME_PREFIX) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read MultiNamePrefix");
        return ERR;
    }

    uint8_t segCount;
    if (aml_seg_count_read(state, &segCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read SegCount");
        return ERR;
    }

    const uint8_t* start = state->current;

    // We just read the NameSegs to verify they are valid.
    aml_name_seg_t* temp;
    for (size_t i = 0; i < segCount; i++)
    {
        if (aml_name_seg_read(state, &temp) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read NameSeg");
            return ERR;
        }
    }

    *outSegments = (aml_name_seg_t*)start;
    *outSegCount = segCount;
    return 0;
}

uint64_t aml_null_name_read(aml_state_t* state)
{
    if (aml_token_expect(state, AML_NULL_NAME) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NullName");
        return ERR;
    }

    return 0;
}

uint64_t aml_name_path_read(aml_state_t* state, aml_name_path_t* out)
{
    aml_token_t firstToken;
    if (aml_token_peek(state, &firstToken) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek token");
        return ERR;
    }

    if (AML_IS_LEAD_NAME_CHAR(&firstToken))
    {
        out->segmentCount = 1;
        return aml_name_seg_read(state, &out->segments);
    }
    else if (firstToken.num == AML_DUAL_NAME_PREFIX)
    {
        out->segmentCount = 2;
        return aml_dual_name_path_read(state, &out->segments);
    }
    else if (firstToken.num == AML_MULTI_NAME_PREFIX)
    {
        return aml_multi_name_path_read(state, &out->segments, &out->segmentCount);
    }
    else if (firstToken.num == AML_NULL_NAME)
    {
        out->segmentCount = 0;
        out->segments = NULL;
        return aml_null_name_read(state);
    }
    else
    {
        AML_DEBUG_ERROR(state, "Invalid name that starts with 0x%x", firstToken.num);
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_prefix_path_read(aml_state_t* state, aml_prefix_path_t* out)
{
    out->depth = 0;
    while (true)
    {
        aml_token_t chr;
        if (aml_token_peek(state, &chr) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to peek value");
            return ERR;
        }

        if (chr.num != AML_PARENT_PREFIX_CHAR)
        {
            return 0;
        }

        aml_state_advance(state, chr.length);
        out->depth++;
    }
}

uint64_t aml_root_char_read(aml_state_t* state, aml_root_char_t* out)
{
    if (aml_token_expect(state, AML_ROOT_CHAR) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read RootChar");
        return ERR;
    }

    out->present = true;
    return 0;
}

uint64_t aml_name_string_read(aml_state_t* state, aml_name_string_t* out)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek token");
        return ERR;
    }

    aml_name_string_t nameString = {0};
    // Starts with either rootchar or prefixpath.
    switch (token.num)
    {
    case AML_ROOT_CHAR:
        if (aml_root_char_read(state, &nameString.rootChar) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read root char");
            return ERR;
        }
        break;
    case AML_PARENT_PREFIX_CHAR:
        if (aml_prefix_path_read(state, &nameString.prefixPath) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read prefix path");
            return ERR;
        }
        break;
    default:
        // Is a empty prefixpath.
        break;
    }

    if (aml_name_path_read(state, &nameString.namePath) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read name path");
        return ERR;
    }

    *out = nameString;
    return 0;
}

aml_object_t* aml_name_string_resolve(aml_name_string_t* nameString, aml_object_t* from)
{
    aml_object_t* start = from;
    if (nameString->rootChar.present)
    {
        start = aml_root_get();
    }

    if (start == NULL)
    {
        return NULL;
    }

    if (!(start->flags & AML_OBJECT_NAMED))
    {
        return NULL;
    }

    aml_object_t* current = start;
    for (uint64_t i = 0; i < nameString->prefixPath.depth; i++)
    {
        current = current->name.parent;
        if (start == NULL)
        {
            return NULL;
        }
    }

    for (uint64_t i = 0; i < nameString->namePath.segmentCount; i++)
    {
        const aml_name_seg_t* segment = &nameString->namePath.segments[i];
        current = aml_object_find_child(current, segment->name);
        if (current == NULL)
        {
            if (start->name.parent != NULL)
            {
                return aml_name_string_resolve(nameString, start->name.parent);
            }
            errno = 0;
            return NULL;
        }
    }

    return current;
}

uint64_t aml_name_string_read_and_resolve(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_name_string_t nameStringLocal;
    if (aml_name_string_read(state, &nameStringLocal) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NameString");
        return ERR;
    }

    *out = aml_name_string_resolve(&nameStringLocal, scope->location);
    if (*out == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    return 0;
}

uint64_t aml_simple_name_read_and_resolve(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read token");
        return ERR;
    }

    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_NAME:
        if (aml_name_string_read_and_resolve(state, scope, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read and resolve NameString");
            return ERR;
        }
        return 0;
    case AML_TOKEN_TYPE_ARG:
        if (aml_arg_obj_read(state, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ArgObj");
            return ERR;
        }
        return 0;
    case AML_TOKEN_TYPE_LOCAL:
        if (aml_local_obj_read(state, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read LocalObj");
            return ERR;
        }
        return 0;
    default:
        AML_DEBUG_ERROR(state, "Invalid token type '%s'", aml_token_type_to_string(token.props->type));
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_super_name_read_and_resolve(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek token");
        return ERR;
    }

    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_NAME:
    case AML_TOKEN_TYPE_ARG:
    case AML_TOKEN_TYPE_LOCAL:
        if (aml_simple_name_read_and_resolve(state, scope, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read and resolve NameString");
            return ERR;
        }
        return 0;
    case AML_TOKEN_TYPE_DEBUG:
        AML_DEBUG_ERROR(state, "DebugObj is unimplemented");
        errno = ENOSYS;
        return ERR;
    case AML_TOKEN_TYPE_EXPRESSION:
        AML_DEBUG_ERROR(state, "ReferenceTypeOpcode is unimplemented");
        errno = ENOSYS;
        return ERR;
    default:
        AML_DEBUG_ERROR(state, "Invalid token type: %d", token.props->type);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_target_read_and_resolve(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek token");
        return ERR;
    }

    if (token.num == AML_NULL_NAME)
    {
        *out = NULL;
        if (aml_null_name_read(state) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read null name");
            return ERR;
        }
    }
    else
    {
        if (aml_simple_name_read_and_resolve(state, scope, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read and resolve simple name");
            return ERR;
        }
    }

    return 0;
}

#include "name.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "data.h"
#include "object_reference.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

uint64_t aml_seg_count_read(aml_state_t* state, aml_seg_count_t* out)
{
    return aml_byte_data_read(state, out);
}

uint64_t aml_name_seg_read(aml_state_t* state, aml_name_seg_t** out)
{
    aml_address_t start = state->pos;

    aml_value_t leadnamechar;
    if (aml_value_read_no_ext(state, &leadnamechar) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read lead name char");
        return ERR;
    }

    if (!AML_IS_LEAD_NAME_CHAR(&leadnamechar))
    {
        AML_DEBUG_ERROR(state, "Invalid lead char 0x%04x in NameSeg", leadnamechar.num);
        errno = EILSEQ;
        return ERR;
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        aml_value_t namechar;
        if (aml_value_read_no_ext(state, &namechar) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read name char");
            return ERR;
        }

        if (!AML_IS_NAME_CHAR(&namechar))
        {
            AML_DEBUG_ERROR(state, "Invalid char 0x%04x in NameSeg", namechar.num);
            errno = EILSEQ;
            return ERR;
        }
    }

    *out = (aml_name_seg_t*)((uint64_t)state->data + start);
    return 0;
}

uint64_t aml_dual_name_path_read(aml_state_t* state, aml_name_seg_t** out)
{
    aml_value_t firstValue;
    if (aml_value_read_no_ext(state, &firstValue) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read first value");
        return ERR;
    }

    if (firstValue.num != AML_DUAL_NAME_PREFIX)
    {
        AML_DEBUG_ERROR(state, "Invalid dual name prefix: 0x%x", firstValue.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_address_t start = state->pos;

    // We just read the two NameSegs to verify they are valid.
    aml_name_seg_t* temp;
    if (aml_name_seg_read(state, &temp) == ERR || aml_name_seg_read(state, &temp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read name seg");
        return ERR;
    }

    *out = (aml_name_seg_t*)((uint64_t)state->data + start);
    return 0;
}

uint64_t aml_multi_name_path_read(aml_state_t* state, aml_name_seg_t** outSegments, aml_seg_count_t* outSegCount)
{
    aml_value_t firstValue;
    if (aml_value_read_no_ext(state, &firstValue) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read first value");
        return ERR;
    }

    if (firstValue.num != AML_MULTI_NAME_PREFIX)
    {
        AML_DEBUG_ERROR(state, "Invalid multi name prefix: 0x%x", firstValue.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_seg_count_t segCount;
    if (aml_seg_count_read(state, &segCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read seg count");
        return ERR;
    }

    aml_address_t start = state->pos;

    // We just read the NameSegs to verify they are valid.
    aml_name_seg_t* temp;
    for (size_t i = 0; i < segCount; i++)
    {
        if (aml_name_seg_read(state, &temp) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read name seg");
            return ERR;
        }
    }

    *outSegments = (aml_name_seg_t*)((uint64_t)state->data + start);
    *outSegCount = segCount;
    return 0;
}

uint64_t aml_null_name_read(aml_state_t* state)
{
    aml_value_t firstValue;
    if (aml_value_read_no_ext(state, &firstValue) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read first value");
        return ERR;
    }

    if (firstValue.num != AML_NULL_NAME)
    {
        AML_DEBUG_ERROR(state, "Invalid null name: 0x%x", firstValue.num);
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_name_path_read(aml_state_t* state, aml_name_path_t* out)
{
    aml_value_t firstValue;
    if (aml_value_peek_no_ext(state, &firstValue) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    if (AML_IS_LEAD_NAME_CHAR(&firstValue))
    {
        out->segmentCount = 1;
        return aml_name_seg_read(state, &out->segments);
    }
    else if (firstValue.num == AML_DUAL_NAME_PREFIX)
    {
        out->segmentCount = 2;
        return aml_dual_name_path_read(state, &out->segments);
    }
    else if (firstValue.num == AML_MULTI_NAME_PREFIX)
    {
        return aml_multi_name_path_read(state, &out->segments, &out->segmentCount);
    }
    else if (firstValue.num == AML_NULL_NAME)
    {
        out->segmentCount = 0;
        out->segments = NULL;
        return aml_null_name_read(state);
    }
    else
    {
        AML_DEBUG_ERROR(state, "Invalid name path start: 0x%x", firstValue.num);
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
        aml_value_t chr;
        if (aml_value_peek_no_ext(state, &chr) == ERR)
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
    aml_value_t rootChar;
    if (aml_value_read_no_ext(state, &rootChar) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read root char");
        return ERR;
    }

    if (rootChar.num != AML_ROOT_CHAR)
    {
        AML_DEBUG_ERROR(state, "Invalid root char: 0x%x", rootChar.num);
        errno = EILSEQ;
        return ERR;
    }

    out->present = true;
    return 0;
}

uint64_t aml_name_string_read(aml_state_t* state, aml_name_string_t* out)
{
    *out = (aml_name_string_t){0};

    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    // Starts with either rootchar or prefixpath.
    switch (value.num)
    {
    case AML_ROOT_CHAR:
        if (aml_root_char_read(state, &out->rootChar) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read root char");
            return ERR;
        }
        break;
    case AML_PARENT_PREFIX_CHAR:
        if (aml_prefix_path_read(state, &out->prefixPath) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read prefix path");
            return ERR;
        }
        break;
    default:
        // Is a empty prefixpath.
        break;
    }

    if (aml_name_path_read(state, &out->namePath) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read name path");
        return ERR;
    }

    return 0;
}

uint64_t aml_simple_name_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out)
{
    aml_value_t value;
    if (aml_value_read(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_NAME:
    {
        aml_name_string_t nameString;
        if (aml_name_string_read(state, &nameString) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read name string");
            return ERR;
        }

        // Target might be NULL, which is still valid.
        aml_node_t* target = aml_node_find(&nameString, node);
        if (aml_object_reference_init(out, target) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init object reference");
            return ERR;
        }
        return 0;
    }
    case AML_VALUE_TYPE_ARG:
        AML_DEBUG_ERROR(state, "Args are unimplemented");
        errno = ENOSYS;
        return ERR;
    case AML_VALUE_TYPE_LOCAL:
        AML_DEBUG_ERROR(state, "Locals are unimplemented");
        errno = ENOSYS;
        return ERR;
    default:
        AML_DEBUG_ERROR(state, "Invalid value type: %d", value.props->type);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_super_name_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_NAME:
    case AML_VALUE_TYPE_ARG:
    case AML_VALUE_TYPE_LOCAL:
        return aml_simple_name_read(state, node, out);
    case AML_VALUE_TYPE_DEBUG:
        AML_DEBUG_ERROR(state, "DebugObj is unimplemented");
        errno = ENOSYS;
        return ERR;
    case AML_VALUE_TYPE_EXPRESSION:
        AML_DEBUG_ERROR(state, "ReferenceTypeOpcode is unimplemented");
        errno = ENOSYS;
        return ERR;
    default:
        AML_DEBUG_ERROR(state, "Invalid value type: %d", value.props->type);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_target_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out)
{
    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    if (value.num == AML_NULL_NAME)
    {
        if (aml_object_reference_init(out, NULL) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init null object reference");
            return ERR;
        }
        return aml_null_name_read(state);
    }
    else
    {
        return aml_super_name_read(state, node, out);
    }
}

#include "name.h"
#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_state.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

uint64_t aml_name_seg_read(aml_state_t* state, aml_name_seg_t* out)
{
    aml_value_t leadnamechar;
    if (aml_value_read_no_ext(state, &leadnamechar) == ERR)
    {
        return ERR;
    }

    if (!AML_IS_LEAD_NAME_CHAR(leadnamechar.num))
    {
        AML_DEBUG_INVALID_STRUCTURE("LeadNameChar");
        errno = EILSEQ;
        return ERR;
    }

    for (int i = 0; i < 3; i++)
    {
        aml_value_t namechar;
        if (aml_value_read_no_ext(state, &namechar) == ERR)
        {
            return ERR;
        }

        if (!AML_IS_NAME_CHAR(namechar.num))
        {
            AML_DEBUG_INVALID_STRUCTURE("NameChar");
            errno = EILSEQ;
            return ERR;
        }

        out->name[i + 1] = namechar.num;
    }

    return 0;
}

uint64_t aml_dual_name_path_read(aml_state_t* state, aml_name_seg_t* firstOut, aml_name_seg_t* secondOut)
{
    aml_value_t firstValue;
    if (aml_value_read_no_ext(state, &firstValue) == ERR)
    {
        return ERR;
    }

    if (firstValue.num != AML_DUAL_NAME_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("DualNamePrefix");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_name_seg_read(state, firstOut) == ERR)
    {
        return ERR;
    }

    if (aml_name_seg_read(state, secondOut) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_seg_count_read(aml_state_t* state, aml_seg_count_t* out)
{
    return aml_byte_data_read(state, out);
}

uint64_t aml_multi_name_path_read(aml_state_t* state, aml_name_seg_t* outSegments, uint8_t* outSegCount)
{
    aml_value_t firstValue;
    if (aml_value_read_no_ext(state, &firstValue) == ERR)
    {
        return ERR;
    }

    if (firstValue.num != AML_MULTI_NAME_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("MultiNamePrefix");
        errno = EILSEQ;
        return ERR;
    }

    aml_seg_count_t segCount;
    if (aml_seg_count_read(state, &segCount) == ERR)
    {
        return ERR;
    }

    for (size_t i = 0; i < segCount; i++)
    {
        if (aml_name_seg_read(state, &outSegments[i]) == ERR)
        {
            return ERR;
        }
    }

    *outSegCount = segCount;
    return 0;
}

uint64_t aml_null_name_read(aml_state_t* state)
{
    aml_value_t firstValue;
    if (aml_value_read_no_ext(state, &firstValue) == ERR)
    {
        return ERR;
    }

    if (firstValue.num != AML_NULL_NAME)
    {
        AML_DEBUG_INVALID_STRUCTURE("NullName");
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
        return ERR;
    }

    if (AML_IS_LEAD_NAME_CHAR(firstValue.num))
    {
        out->segmentCount = 1;
        return aml_name_seg_read(state, &out->segments[0]);
    }
    else if (firstValue.num == AML_DUAL_NAME_PREFIX)
    {
        out->segmentCount = 2;
        return aml_dual_name_path_read(state, &out->segments[0], &out->segments[1]);
    }
    else if (firstValue.num == AML_MULTI_NAME_PREFIX)
    {
        return aml_multi_name_path_read(state, out->segments, &out->segmentCount);
    }
    else if (firstValue.num == AML_NULL_NAME)
    {
        out->segmentCount = 0;
        return aml_null_name_read(state);
    }
    else
    {
        AML_DEBUG_INVALID_STRUCTURE("NamePath");
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_root_char_read(aml_state_t* state, aml_root_char_t* out)
{
    aml_value_t rootChar;
    if (aml_value_read_no_ext(state, &rootChar) == ERR)
    {
        return ERR;
    }

    if (rootChar.num != AML_ROOT_CHAR)
    {
        AML_DEBUG_INVALID_STRUCTURE("RootChar");
        errno = EILSEQ;
        return ERR;
    }

    out->present = true;
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

uint64_t aml_name_string_read(aml_state_t* state, aml_name_string_t* out)
{
    *out = (aml_name_string_t){0};

    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        return ERR;
    }

    // Starts with either rootchar or prefixpath.
    switch (value.num)
    {
    case AML_ROOT_CHAR:
        if (aml_root_char_read(state, &out->rootChar) == ERR)
        {
            return ERR;
        }
        break;
    case AML_PARENT_PREFIX_CHAR:
        if (aml_prefix_path_read(state, &out->prefixPath) == ERR)
        {
            return ERR;
        }
        break;
    default:
        // Is a empty prefixpath.
        break;
    }

    if (aml_name_path_read(state, &out->namePath) == ERR)
    {
        return ERR;
    }

    return 0;
}

aml_node_t* aml_name_string_walk(const aml_name_string_t* nameString, aml_node_t* start)
{
    if (start == NULL || nameString->rootChar.present)
    {
        start = aml_root_get();
    }

    for (uint64_t i = 0; i < nameString->prefixPath.depth; i++)
    {
        start = start->parent;
        if (start == NULL)
        {
            errno = ENOENT;
            return NULL;
        }
    }

    aml_node_t* found = start;
    for (uint64_t i = 0; i < nameString->namePath.segmentCount; i++)
    {
        aml_node_t* next = NULL;
        const aml_name_seg_t* segment = &nameString->namePath.segments[i];
        aml_node_t* child = NULL;
        LIST_FOR_EACH(child, &start->children, entry)
        {
            if (memcmp(child->name, segment->name, AML_NAME_LENGTH_SEG) == 0)
            {
                next = child;
                break;
            }
        }

        if (next == NULL)
        {
            errno = ENOENT;
            return NULL;
        }
        found = next;
    }

    return found;
}

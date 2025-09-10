#include "name.h"
#include "acpi/aml/aml.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_node.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

uint64_t aml_root_char_read(aml_state_t* state, aml_root_char_t* out)
{
    uint64_t byte = aml_state_byte_read(state);
    if (byte == ERR)
    {
        return ERR;
    }

    if (byte != AML_ROOT_CHAR)
    {
        errno = EILSEQ;
        return ERR;
    }

    out->present = true;
    return 0;
}

uint64_t aml_name_seg_read(aml_state_t* state, aml_name_seg_t* out)
{
    uint64_t leadnamechar = aml_state_byte_read(state);
    if (leadnamechar == ERR)
    {
        return ERR;
    }

    if (!AML_IS_LEAD_NAME_CHAR(leadnamechar))
    {
        errno = EILSEQ;
        return ERR;
    }

    for (int i = 0; i < 3; i++)
    {
        uint64_t byte = aml_state_byte_read(state);
        if (byte == ERR)
        {
            return ERR;
        }

        if (!AML_IS_NAME_CHAR(byte))
        {
            errno = EILSEQ;
            return ERR;
        }

        out->name[i + 1] = byte;
    }

    return 0;
}

uint64_t aml_dual_name_path_read(aml_state_t* state, aml_name_seg_t* firstOut, aml_name_seg_t* secondOut)
{
    uint64_t firstByte = aml_state_byte_read(state);
    if (firstByte == ERR)
    {
        return ERR;
    }

    if (firstByte != AML_DUAL_NAME_PREFIX)
    {
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

uint64_t aml_multi_name_path_read(aml_state_t* state, aml_name_seg_t* outSegments, uint8_t* outSegCount)
{
    uint64_t firstByte = aml_state_byte_read(state);
    if (firstByte == ERR)
    {
        return ERR;
    }

    if (firstByte != AML_MULTI_NAME_PREFIX)
    {
        errno = EILSEQ;
        return ERR;
    }

    uint64_t segCount = aml_state_byte_read(state);
    if (segCount == ERR)
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
    uint64_t firstByte = aml_state_byte_read(state);
    if (firstByte == ERR)
    {
        return ERR;
    }

    if (firstByte != AML_NULL_NAME)
    {
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_name_path_read(aml_state_t* state, aml_name_path_t* out)
{
    uint64_t firstByte = aml_state_byte_peek(state);
    if (firstByte == ERR)
    {
        return ERR;
    }

    if (AML_IS_LEAD_NAME_CHAR(firstByte))
    {
        out->segmentCount = 1;
        return aml_name_seg_read(state, &out->segments[0]);
    }
    else if (firstByte == AML_DUAL_NAME_PREFIX)
    {
        out->segmentCount = 2;
        return aml_dual_name_path_read(state, &out->segments[0], &out->segments[1]);
    }
    else if (firstByte == AML_MULTI_NAME_PREFIX)
    {
        return aml_multi_name_path_read(state, out->segments, &out->segmentCount);
    }
    else if (firstByte == AML_NULL_NAME)
    {
        out->segmentCount = 0;
        return aml_null_name_read(state);
    }

    return 0;
}

uint64_t aml_prefix_path_read(aml_state_t* state, aml_prefix_path_t* out)
{
    out->depth = 0;
    while (true)
    {
        uint64_t byte = aml_state_byte_read(state);
        if (byte == ERR)
        {
            return ERR;
        }

        if (byte != AML_PARENT_PREFIX_CHAR)
        {
            return 0;
        }

        out->depth++;
    }
}

uint64_t aml_name_string_read(aml_state_t* state, aml_name_string_t* out)
{
    *out = (aml_name_string_t){0};

    uint64_t firstByte = aml_state_byte_peek(state);
    if (firstByte == ERR)
    {
        return ERR;
    }

    switch (firstByte)
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
    if (nameString == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

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

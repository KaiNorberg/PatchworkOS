#pragma once

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_state.h"
#include "data.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>

/**
 * @brief ACPI AML Name Objects Encoding
 * @defgroup kernel_acpi_aml_name Name Objects
 * @ingroup kernel_acpi_aml
 *
 * Not to be confused with "ACPI AML Named Objects Encoding".
 *
 * See section 20.2.2 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Maximum number of segments in a name path.
 */
#define AML_MAX_NAME_PATH 254

/**
 * @brief The exact length of a aml name not including a null character.
 */
#define AML_NAME_LENGTH 4

/**
 * @brief Check if a value is a LeadNameChar structure.
 *
 * @param value The value to check.
 * @return true if the value is a LeadNameChar structure, false otherwise.
 */
#define AML_IS_LEAD_NAME_CHAR(value) \
    (((value)->num >= AML_NAME_CHAR_A && (value)->num <= AML_NAME_CHAR_Z) || (value)->num == AML_NAME_CHAR)

/**
 * @brief Check if a value is a DigitChar structure.
 *
 * @param value The value to check.
 * @return true if the value is a DigitChar structure, false otherwise.
 */
#define AML_IS_DIGIT_CHAR(value) (((value)->num >= AML_DIGIT_CHAR_0 && (value)->num <= AML_DIGIT_CHAR_9))

/**
 * @brief Check if a value is a NameChar structure.
 *
 * @param value The value to check.
 * @return true if the value is a NameChar structure, false otherwise.
 */
#define AML_IS_NAME_CHAR(value) (AML_IS_DIGIT_CHAR(value) || AML_IS_LEAD_NAME_CHAR(value))

/**
 * @brief A PrefixPath structure.
 * @struct aml_prefix_path_t
 */
typedef struct
{
    uint16_t depth; //!< Number of parent prefixes ('^') in the prefix, each prefix means go back one level in the
                    //! namespace hierarchy.
} aml_prefix_path_t;

/**
 * @brief A RootChar structure.
 * @struct aml_root_char_t
 */
typedef struct
{
    bool present; //!< If the first character is a root character ('\\'), if yes, the name string is absolute.
} aml_root_char_t;

/**
 * @brief A NameSeg strcture.
 * @struct aml_name_seg_t
 */
typedef struct
{
    char name[AML_NAME_LENGTH + 1];
} aml_name_seg_t;

/**
 * @brief Represents the NamePath, DualNamePath, MultiNamePath and NullPath structures.
 * @struct aml_name_path_t
 */
typedef struct
{
    aml_name_seg_t segments[AML_MAX_NAME_PATH]; //!< Array of segments in the name string.
    uint8_t segmentCount;                       //!< Number of segments in the name string.
} aml_name_path_t;

/**
 * @brief A NameString structure.
 * @struct aml_name_string_t
 */
typedef struct
{
    aml_root_char_t rootChar;
    aml_prefix_path_t prefixPath;
    aml_name_path_t namePath;
} aml_name_string_t;

/**
 * @brief Reads the next data as a NameSeg from the AML bytecode stream.
 *
 * A NameSeg structure is defined as `NameSeg := <leadnamechar namechar namechar namechar>`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the NameSeg will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_name_seg_read(aml_state_t* state, aml_name_seg_t* out)
{
    aml_value_t leadnamechar;
    if (aml_value_read_no_ext(state, &leadnamechar) == ERR)
    {
        return ERR;
    }

    if (!AML_IS_LEAD_NAME_CHAR(&leadnamechar))
    {
        AML_DEBUG_INVALID_STRUCTURE("LeadNameChar");
        errno = EILSEQ;
        return ERR;
    }

    out->name[0] = leadnamechar.num;
    for (int i = 0; i < 3; i++)
    {
        aml_value_t namechar;
        if (aml_value_read_no_ext(state, &namechar) == ERR)
        {
            return ERR;
        }

        if (!AML_IS_NAME_CHAR(&namechar))
        {
            AML_DEBUG_INVALID_STRUCTURE("NameChar");
            errno = EILSEQ;
            return ERR;
        }

        out->name[i + 1] = namechar.num;
    }
    out->name[4] = '\0';

    return 0;
}

/**
 * @brief Reads the next data as a DualNamePath structure from the AML bytecode stream.
 *
 * A DualNamePath structure is defined as `DualNamePath := DualNamePrefix NameSeg NameSeg`.
 *
 * @param state The AML state.
 * @param firstOut Pointer to destination where the first segment of the DualNamePath will be stored.
 * @param secondOut Pointer to destination where the second segment of the DualNamePath will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_dual_name_path_read(aml_state_t* state, aml_name_seg_t* firstOut, aml_name_seg_t* secondOut)
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

/**
 * @brief ACPI AML SegCount structure.
 */
typedef aml_byte_data_t aml_seg_count_t;

/**
 * @brief Reads the next data as a SegCount structure from the AML bytecode stream.
 *
 * A SegCount structure is defined as `SegCount := ByteData`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the SegCount will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_seg_count_read(aml_state_t* state, aml_seg_count_t* out)
{
    return aml_byte_data_read(state, out);
}

/**
 * @brief Reads the next data as a MultiNamePath structure from the AML bytecode stream.
 *
 * A MultiNamePath structure is defined as `MultiNamePath := MultiNamePrefix SegCount NameSeg(SegCount)`.
 *
 * @param state The AML state.
 * @param outSegments Pointer to destination where the segments of the MultiNamePath will be stored.
 * @param outSegCount Pointer to destination where the number of segments will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_multi_name_path_read(aml_state_t* state, aml_name_seg_t* outSegments, uint8_t* outSegCount)
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

/**
 * Reads the next data as a NullName structure from the AML bytecode stream.
 *
 * A NullName structure is defined as `NullName := 0x00`.
 *
 * @param state The AML state.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_null_name_read(aml_state_t* state)
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

/**
 * @brief Reads the next data as a NamePath structure from the AML bytecode stream.
 *
 * A NamePath structure is defined as `NamePath := NameSeg | DualNamePath | MultiNamePath | NullName`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the NamePath will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_name_path_read(aml_state_t* state, aml_name_path_t* out)
{
    aml_value_t firstValue;
    if (aml_value_peek_no_ext(state, &firstValue) == ERR)
    {
        return ERR;
    }

    if (AML_IS_LEAD_NAME_CHAR(&firstValue))
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

/**
 * @brief Reads the next data as a PrefixPath structure from the AML bytecode stream.
 *
 * A PrefixPath structure is defined as `PrefixPath := Nothing | <'^' prefixpath>`.
 *
 * Note that `^` is just a `AML_PARENT_PREFIX_CHAR`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the PrefixPath will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_prefix_path_read(aml_state_t* state, aml_prefix_path_t* out)
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

/**
 * @brief Reads the next data as a RootChar from the AML bytecode stream.
 *
 * A RootChar is defined as `RootChar := 0x5C`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the RootChar will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_root_char_read(aml_state_t* state, aml_root_char_t* out)
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

/**
 * @brief Reads the next data as a NameString structure from the AML bytecode stream.
 *
 * A NameString structure is defined as `NameString := <rootchar namepath> | <prefixpath namepath>`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the name string will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_name_string_read(aml_state_t* state, aml_name_string_t* out)
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

/**
 * @brief Walks the ACPI namespace tree to find the node corresponding to the given NameString.
 *
 * @param nameString The NameString to search for.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @return On success, a pointer to the found node. On error, `NULL` and `errno` is set.
 */
static inline aml_node_t* aml_name_string_walk(const aml_name_string_t* nameString, aml_node_t* start)
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
            if (memcmp(child->name, segment->name, AML_NAME_LENGTH) == 0)
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

/**
 * @brief Add a new node at the location and with the name specified by the NameString.
 *
 * @param string The Namestring specifying the parent node.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @param type The type of the new node.
 * @return On success, a pointer to the new node. On error, `NULL` and `errno` is set.
 */
static inline aml_node_t* aml_add_node_at_name_string(aml_name_string_t* string, aml_node_t* start,
    aml_node_type_t type)
{
    if (string->namePath.segmentCount == 0)
    {
        errno = EILSEQ;
        return NULL;
    }

    if (start == NULL || string->rootChar.present)
    {
        start = aml_root_get();
    }

    for (uint64_t i = 0; i < string->prefixPath.depth; i++)
    {
        start = start->parent;
        if (start == NULL)
        {
            errno = ENOENT;
            return NULL;
        }
    }

    aml_node_t* parentNode = start;
    for (uint64_t i = 1; i < string->namePath.segmentCount; i++)
    {
        aml_node_t* next = NULL;
        const aml_name_seg_t* segment = &string->namePath.segments[i - 1];
        aml_node_t* child = NULL;
        LIST_FOR_EACH(child, &start->children, entry)
        {
            if (memcmp(child->name, segment->name, AML_NAME_LENGTH) == 0)
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
        parentNode = next;
    }

    aml_node_t* newNode =
        aml_add_node(parentNode, string->namePath.segments[string->namePath.segmentCount - 1].name, type);
    if (newNode == NULL)
    {
        return NULL;
    }

    return newNode;
}

/** @} */

#pragma once

#include "aml_state.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief ACPI AML State
 * @defgroup kernel_acpi_aml_parse State
 * @ingroup kernel_acpi_aml
 *
 * The ACPI AML State is used to keep track of the virtual machine's state during the parsing of AML bytecode and
 * provides wrappers to read data from the ACPI AML stream.
 *
 * @{
 */

/**
 * @brief AML State
 * @struct aml_state_t
 *
 * Used in the `aml_parse()` function to keep track of the virtual machine's state.
 */
typedef struct aml_state
{
    const void* data;            //!< Pointer to the AML bytecode stream.
    uint64_t dataSize;           //!< Size of the AML bytecode stream.
    uint64_t instructionPointer; //!< Index of the current instruction in the AML bytecode stream.
} aml_state_t;

/**
 * @brief Reads the next raw byte from the AML bytecode stream and advances the instruction pointer.
 *
 * @param state The AML state.
 * @return On success, the next byte. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_read_byte(aml_state_t* state)
{
    if (state->instructionPointer >= state->dataSize)
    {
        errno = ENODATA;
        return ERR;
    }

    return ((uint8_t*)state->data)[state->instructionPointer++];
}

/**
 * @brief Reads the next raw byte from the AML bytecode stream without advancing the instruction pointer.
 *
 * @param state The AML state.
 * @return On success, the next byte. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_peek_byte(aml_state_t* state)
{
    if (state->instructionPointer >= state->dataSize)
    {
        errno = ENODATA;
        return ERR;
    }

    return ((uint8_t*)state->data)[state->instructionPointer];
}

/**
 * @brief Reads the next raw bytes from the AML bytecode stream.
 *
 * @param state The AML state.
 * @param buffer Pointer to destination where the bytes will be stored.
 * @param count The number of bytes to read.
 * @return The number of bytes read.
 */
static inline uint64_t aml_read_bytes(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->dataSize - state->instructionPointer;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, (uint8_t*)state->data + state->instructionPointer, count);
    state->instructionPointer += count;
    return count;
}

/**
 * @brief Reads the next raw bytes from the AML bytecode stream without advancing the instruction pointer.
 *
 * @param state The AML state.
 * @param buffer Pointer to destination where the bytes will be stored.
 * @param count The number of bytes to read.
 * @return The number of bytes read.
 */
static inline uint64_t aml_peek_bytes(aml_state_t* state, uint8_t* buffer, uint64_t count)
{
    uint64_t bytesAvailable = state->dataSize - state->instructionPointer;
    if (count > bytesAvailable)
    {
        count = bytesAvailable;
    }

    memcpy(buffer, (uint8_t*)state->data + state->instructionPointer, count);
    return count;
}

/**
 * @brief Advances the instruction pointer by the specified offset.
 *
 * @param state The AML state.
 * @param offset The number of bytes to advance.
 * @return The new instruction pointer.
 */
static inline uint64_t aml_advance(aml_state_t* state, uint64_t offset)
{
    uint64_t bytesAvailable = state->dataSize - state->instructionPointer;
    if (offset > bytesAvailable)
    {
        offset = bytesAvailable;
    }

    state->instructionPointer += offset;
    return state->instructionPointer;
}

#define AML_NAME_STRING_MAX_SEGMENTS 254
#define AML_NAME_STRING_SEGMENT_LENGTH 4

/**
 * @brief Represents one name in the ACPI hierarchy, for example `PCI0`.
 * @struct aml_name_seg_t
 */
typedef struct
{
    uint8_t name[AML_NAME_STRING_SEGMENT_LENGTH];
} aml_name_seg_t;

#define AML_IS_LEAD_NAME_CHAR(c) ((c >= 'A' && c <= 'Z') || c == '_')
#define AML_IS_ROOT_CHAR(c) ((c == '\\'))
#define AML_IS_PARENT_PREFIX_CHAR(c) ((c == '^'))
#define AML_IS_DIGIT_CHAR(c) ((c >= '0' && c <= '9'))
#define AML_IS_NAME_CHAR(c) (AML_IS_LEAD_NAME_CHAR(c) || AML_IS_DIGIT_CHAR(c))
#define AML_IS_DUAL_NAME_PREFIX(c) ((c == 0x2E))
#define AML_IS_MULTI_NAME_PREFIX(c) ((c == 0x2F))
#define AML_IS_NULL_NAME(c) ((c == 0x00))

/**
 * @brief Reads the next data as a name segment from the AML bytecode stream.
 *
 * See section 20.2.2 of the ACPI specification.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the name segment will be stored.
 * @param firstChar The first character of the name segment. Some functions might have already read the first character
 * to know if the data is a name segment before calling this function, they should pass it here. Can be NULL.
 * @return On success, the number of bytes read. On error, `ERR` and `errno` is set.
 */
/*static inline uint64_t aml_state_read_name_seg(aml_state_t* state, aml_name_seg_t* out, uint8_t* firstChar)
{
    uint64_t bytesRead = 0;
    if (firstChar != NULL)
    {
        out->name[0] = *firstChar;
    }
    else
    {
        if (!aml_state_read_byte(state, &out->name[0]))
        {
            errno = ENODATA;
            return ERR;
        }

        if (!AML_IS_LEAD_NAME_CHAR(out->name[0]))
        {
            errno = EILSEQ;
            return ERR;
        }

        bytesRead++;
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        if (!aml_state_read_byte(state, &out->name[i + 1]))
        {
            errno = ENODATA;
            return ERR;
        }

        if (!AML_IS_NAME_CHAR(out->name[i + 1]))
        {
            errno = EILSEQ;
            return ERR;
        }

        bytesRead++;
    }

    return bytesRead;
}*/

/**
 * @brief Represents a name string in the ACPI namespace, for example `\\_SB.PCI0.SEC0`.
 * @struct aml_name_string_t
 */
typedef struct
{
    aml_name_seg_t segments[AML_NAME_STRING_MAX_SEGMENTS]; //!< Array of segments in the name string.
    uint8_t segmentCount;                                  //!< Number of segments in the name string.
    bool startFromRoot; //!< If the first character is a root character ('\\'), if yes, the name string is absolute.
    uint8_t parentPrefixCount; //!< Number of parent prefixes ('^') at the start of the string, each prefix means go
                               //!< back one level in the namespace hierarchy.
} aml_name_string_t;

/**
 * @brief Reads the next data as a name string object from the AML bytecode stream.
 *
 * See section 20.2.2 of the ACPI specification.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the name string will be stored.
 * @return On success, the number of bytes read. On error, `ERR` and `errno` is set.
 */
/*static inline uint64_t aml_state_read_name_string(aml_state_t* state, aml_name_string_t* out)
{
    uint8_t byte;
    if (!aml_state_read_byte(state, &byte))
    {
        errno = ENODATA;
        return ERR;
    }

    *out = (aml_name_string_t){
        .segments = {},
        .segmentCount = 0,
        .startFromRoot = false,
        .parentPrefixCount = 0,
    };
    uint64_t bytesRead = 1;

    // Name string starts with either a rootchar or prefix path, the prefix path can be nothing, followed by a namepath.

    if (AML_IS_ROOT_CHAR(byte))
    {
        out->startFromRoot = true;
        if (!aml_state_read_byte(state, &byte))
        {
            errno = ENODATA;
            return ERR;
        }
        bytesRead++;
    }
    else
    {
        while (AML_IS_PARENT_PREFIX_CHAR(byte))
        {
            out->parentPrefixCount++;
            if (!aml_state_read_byte(state, &byte))
            {
                errno = ENODATA;
                return ERR;
            }
            bytesRead++;
        }
    }

    if (AML_IS_LEAD_NAME_CHAR(byte)) // Name string is a just a NameSeg, and we already read the first leadnamechar:
                                     // <leadnamechar namechar namechar namechar>
    {
        uint64_t nameSegBytes = aml_state_read_name_seg(state, &out->segments[0], &byte);
        if (nameSegBytes == ERR)
        {
            return ERR;
        }
        bytesRead += nameSegBytes;
        out->segmentCount = 1;

        return bytesRead;
    }
    else if (AML_IS_DUAL_NAME_PREFIX(byte)) // Name string is two NameSegs: <leadnamechar namechar namechar namechar>
                                            // <leadnamechar namechar namechar namechar>
    {
        uint64_t nameSegBytes = aml_state_read_name_seg(state, &out->segments[0], NULL);
        if (nameSegBytes == ERR)
        {
            return ERR;
        }
        bytesRead += nameSegBytes;
        out->segmentCount = 1;

        nameSegBytes = aml_state_read_name_seg(state, &out->segments[1], NULL);
        if (nameSegBytes == ERR)
        {
            return ERR;
        }
        bytesRead += nameSegBytes;
        out->segmentCount = 2;

        return bytesRead;
    }
    else if (AML_IS_MULTI_NAME_PREFIX(byte)) // Name string is multiple NameSegs: SegCount <leadnamechar namechar
                                             // namechar namechar>(SegCount)
    {
        uint8_t segCount = 0;
        if (!aml_state_read_byte(state, &segCount))
        {
            errno = ENODATA;
            return ERR;
        }

        if (segCount == 0)
        {
            errno = EILSEQ;
            return ERR;
        }

        for (uint8_t i = 0; i < segCount; i++)
        {
            uint64_t nameSegBytes = aml_state_read_name_seg(state, &out->segments[i], NULL);
            if (nameSegBytes == ERR)
            {
                return ERR;
            }
            bytesRead += nameSegBytes;
        }
        out->segmentCount = segCount;

        return bytesRead;
    }
    else if (AML_IS_NULL_NAME(byte)) // Name string is null: <null>
    {
        out->segmentCount = 0;
        return bytesRead;
    }

    errno = EILSEQ;
    return ERR;
}*/

/** @} */

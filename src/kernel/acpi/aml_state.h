#pragma once

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief ACPI AML State
 * @defgroup kernel_acpi_aml_parse AML State
 * @ingroup kernel
 *
 * The ACPI AML State is used to keep track of the virtual machine's state during the parsing of AML bytecode and provides wrappers to read data from the ACPI AML stream.
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
    uint64_t instructionPointer; //!< The index into the data array at which the next instruction will be fetched.
    const void* data;            //!< Pointer to the AML bytecode stream.
    uint64_t dataSize;           //!< Size of the AML bytecode stream.
} aml_state_t;

/**
 * @brief Reads the next raw byte from the AML bytecode stream.
 *
 * @param state The AML state.
 * @param out Pointer to the byte that will be read.
 * @return The number of bytes read.
 */
static inline uint64_t aml_state_read_byte(aml_state_t* state, uint8_t* out)
{
    if (state->instructionPointer >= state->dataSize)
    {
        return 0; // End of file.
    }

    *out = ((uint8_t*)state->data)[state->instructionPointer++];
    return 1; // One byte read.
}

typedef uint64_t aml_pkg_length_t;

/**
 * @brief Reads the next data as a package length object from the AML bytecode stream.
 *
 * See section 20.2.4 of the ACPI specification.
 *
 * @param state The AML state.Mayb
 * @param out Pointer to destination where the package length will be stored.
 * @return On success, the number of bytes read. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_state_read_pkg_length(aml_state_t* state, aml_pkg_length_t* out)
{
    uint8_t pkgLeadByte;
    if (aml_state_read_byte(state, &pkgLeadByte) == 0)
    {
        errno = ENODATA;
        return ERR;
    }
    uint64_t bytesRead = 1;

    uint8_t bytedataCount = (pkgLeadByte >> 6) & 0b11; // bits (7-6)

    // If no bytes follow, then the first 6 bits store the package length.
    if (bytedataCount == 0)
    {
        *out = pkgLeadByte & 0b111111;
        return 1;
    }

    // If more bytes follow, then bits 4 and 5 must be zero.
    if (pkgLeadByte & (1 << 4) || pkgLeadByte & (1 << 5))
    {
        errno = EILSEQ;
        return ERR;
    }

    // Bits 0 to 3 in pkgLeadByte becomes the least significant bits in the length, followed by the next bytes.
    *out = (pkgLeadByte & 0b1111);
    for (uint8_t i = 0; i < bytedataCount; i++)
    {
        uint8_t byte;
        if (aml_state_read_byte(state, &byte) == 0)
        {
            errno = ENODATA;
            return ERR;
        }
        bytesRead += 1;
        *out |= ((uint64_t)byte) << (i * 8 + 4);
    }

    // Output must not be greater than 2^28.
    if (*out > (1ULL << 28))
    {
        errno = ERANGE;
        return ERR;
    }

    return bytesRead;
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
 * @param firstChar The first character of the name segment. Some functions might have already read the first character to know if the data is a name segment before calling this function, they should pass it here. Can be NULL.
 * @return On success, the number of bytes read. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_state_read_name_seg(aml_state_t* state, aml_name_seg_t* out, uint8_t* firstChar)
{
    uint64_t bytesRead = 0;
    if (firstChar != NULL)
    {
        out->name[0] = *firstChar;
    }
    else
    {
        if (aml_state_read_byte(state, &out->name[0]) == 0)
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
        if (aml_state_read_byte(state, &out->name[i + 1]) == 0)
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
}

/**
 * @brief Represents a name string in the ACPI namespace, for example `\\_SB.PCI0.SEC0`.
 * @struct aml_name_string_t
 */
typedef struct
{
    aml_name_seg_t segments[AML_NAME_STRING_MAX_SEGMENTS]; //!< Array of segments in the name string.
    uint8_t segmentCount; //!< Number of segments in the name string.
    bool startFromRoot; //!< If the first character is a root character ('\\'), if yes, the name string is absolute.
    uint8_t parentPrefixCount; //!< Number of parent prefixes ('^') at the start of the string, each prefix means go back one level in the namespace hierarchy.
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
static inline uint64_t aml_state_read_name_string(aml_state_t* state, aml_name_string_t* out)
{
    uint8_t byte;
    if (aml_state_read_byte(state, &byte) == 0)
    {
        errno = ENODATA;
        return ERR;
    }

    *out = (aml_name_string_t) {
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
        if (aml_state_read_byte(state, &byte) == 0)
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
            if (aml_state_read_byte(state, &byte) == 0)
            {
                errno = ENODATA;
                return ERR;
            }
            bytesRead++;
        }
    }

    if (AML_IS_LEAD_NAME_CHAR(byte)) // Name string is a just a NameSeg, and we already read the first leadnamechar: <leadnamechar namechar namechar namechar>
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
    else if (AML_IS_DUAL_NAME_PREFIX(byte)) // Name string is two NameSegs: <leadnamechar namechar namechar namechar> <leadnamechar namechar namechar namechar>
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
    else if (AML_IS_MULTI_NAME_PREFIX(byte)) // Name string is multiple NameSegs: SegCount <leadnamechar namechar namechar namechar>(SegCount)
    {
        uint8_t segCount = 0;
        if (aml_state_read_byte(state, &segCount))
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
}

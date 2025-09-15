#pragma once

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"

#include "expression.h"

#include <errno.h>
#include <stdint.h>

/**
 * @brief ACPI AML Data Objects Encoding
 * @defgroup kernel_acpi_aml_data Data Objects
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.3 of the ACPI specification for more details.
 *
 * @{
 */

#include "data_types.h"

/**
 * @brief Read a ByteData structure from the AML stream.
 *
 * A ByteData structure is defined as `ByteData := 0x00 - 0xFF`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ByteData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_byte_data_read(aml_state_t* state, aml_byte_data_t* out)
{
    uint8_t byte;
    if (aml_state_read(state, &byte, 1) != 1)
    {
        AML_DEBUG_INVALID_STRUCTURE("ByteData");
        errno = ENODATA;
        return ERR;
    }
    *out = byte;
    return 0;
}

/**
 * @brief Read a WordData structure from the AML stream.
 *
 * A WordData structure is defined as `WordData := ByteData[0:7] ByteData[8:15]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the WordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_word_data_read(aml_state_t* state, aml_word_data_t* out)
{
    aml_byte_data_t byte1, byte2;
    if (aml_byte_data_read(state, &byte1) == ERR || aml_byte_data_read(state, &byte2) == ERR)
    {
        AML_DEBUG_INVALID_STRUCTURE("WordData");
        return ERR;
    }
    *out = ((aml_word_data_t)byte1) | (((aml_word_data_t)byte2) << 8);
    return 0;
}

/**
 * @brief Read a DWordData structure from the AML stream.
 *
 * A DWordData structure is defined as `DWordData := WordData[0:15] WordData[16:31]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_dword_data_read(aml_state_t* state, aml_dword_data_t* out)
{
    aml_word_data_t word1, word2;
    if (aml_word_data_read(state, &word1) == ERR || aml_word_data_read(state, &word2) == ERR)
    {
        AML_DEBUG_INVALID_STRUCTURE("DWordData");
        return ERR;
    }
    *out = ((aml_dword_data_t)word1) | (((aml_dword_data_t)word2) << 16);
    return 0;
}

/**
 * @brief Read a QWordData structure from the AML stream.
 *
 * A QWordData structure is defined as `QWordData := DWordData[0:31] DWordData[32:63]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the QWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_qword_data_read(aml_state_t* state, aml_qword_data_t* out)
{
    aml_dword_data_t dword1, dword2;
    if (aml_dword_data_read(state, &dword1) == ERR || aml_dword_data_read(state, &dword2) == ERR)
    {
        AML_DEBUG_INVALID_STRUCTURE("QWordData");
        return ERR;
    }
    *out = ((aml_qword_data_t)dword1) | (((aml_qword_data_t)dword2) << 32);
    return 0;
}

/**
 * @brief Read a ByteConst structure from the AML stream.
 *
 * A ByteConst structure is defined as `ByteConst := BytePrefix ByteData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ByteData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_byte_const_read(aml_state_t* state, aml_byte_const_t* out)
{
    aml_value_t prefix;
    if (aml_value_read(state, &prefix) == ERR)
    {
        return ERR;
    }

    if (prefix.num != AML_BYTE_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("ByteConst");
        errno = EILSEQ;
        return ERR;
    }

    return aml_byte_data_read(state, out);
}

/**
 * @brief Read a WordConst structure from the AML stream.
 *
 * A WordConst structure is defined as `WordConst := WordPrefix WordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the WordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_word_const_read(aml_state_t* state, aml_word_const_t* out)
{
    aml_value_t prefix;
    if (aml_value_read(state, &prefix) == ERR)
    {
        return ERR;
    }

    if (prefix.num != AML_WORD_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("WordConst");
        errno = EILSEQ;
        return ERR;
    }

    return aml_word_data_read(state, out);
}

/**
 * @brief Read a DWordConst structure from the AML stream.
 *
 * A DWordConst structure is defined as `DwordConst := DWordPrefix DWordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_dword_const_read(aml_state_t* state, aml_dword_const_t* out)
{
    aml_value_t prefix;
    if (aml_value_read(state, &prefix) == ERR)
    {
        return ERR;
    }

    if (prefix.num != AML_DWORD_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("DWordConst");
        errno = EILSEQ;
        return ERR;
    }

    return aml_dword_data_read(state, out);
}

/**
 * @brief Read a QWordConst structure from the AML stream.
 *
 * A QWordConst structure is defined as `QWordConst := QWordPrefix QWordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the QWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_qword_const_read(aml_state_t* state, aml_qword_const_t* out)
{
    aml_value_t prefix;
    if (aml_value_read(state, &prefix) == ERR)
    {
        return ERR;
    }

    if (prefix.num != AML_QWORD_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("QWordConst");
        errno = EILSEQ;
        return ERR;
    }

    return aml_qword_data_read(state, out);
}

/**
 * @brief Read a ConstObj structure from the AML stream.
 *
 * A ConstObj structure is defined as `ConstObj := ZeroOp | OneOp | OnesOp`.
 *
 * See sections 19.6.98, 19.6.99 and 19.6.156 for more details.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ConstObj will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_const_obj_read(aml_state_t* state, aml_const_obj_t* out)
{
    aml_value_t value;
    if (aml_value_read_no_ext(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_ZERO_OP:
        *out = 0;
        return 0;
    case AML_ONE_OP:
        *out = 1;
        return 0;
    case AML_ONES_OP:
        *out = ~0;
        return 0;
    default:
        AML_DEBUG_INVALID_STRUCTURE("ConstObj");
        errno = EILSEQ;
        return ERR;
    }
}



/**
 * @brief Read a String structure from the AML stream.
 *
 * A String structure is defined as `String := StringPrefix AsciiCharList NullChar`.
 *
 * AsciiCharList is defined as a sequence of ASCII characters in the range 0x01 to 0x7F, and NullChar is defined as 0x00.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the string will be stored. This will point to a location within the AML bytestream and should not be freed or modified.
 * @param outLength Pointer to a variable where the length of the string (excluding the null terminator) will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_string_read(aml_state_t* state, char** out, uint64_t* outLength)
{
    aml_value_t stringPrefix;
    if (aml_value_read(state, &stringPrefix) == ERR)
    {
        return ERR;
    }

    if (stringPrefix.num != AML_STRING_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("String");
        errno = EILSEQ;
        return ERR;
    }

    char* str = (char*)((uint64_t)state->data + (uint64_t)state->pos);
    uint64_t length = 0;
    while (1)
    {
        uint8_t c;
        if (aml_state_read(state, &c, 1) != 1)
        {
            AML_DEBUG_INVALID_STRUCTURE("String: Unexpected end of stream");
            errno = ENODATA;
            return ERR;
        }

        if (c == 0x00)
        {
            break;
        }

        if (c < 0x01 || c > 0x7F)
        {
            AML_DEBUG_INVALID_STRUCTURE("String: Non-ASCII character encountered");
            errno = EILSEQ;
            return ERR;
        }

        length++;
    }

    *out = str;
    *outLength = length;
    return 0;
}

/**
 * @brief Read a ComputationalData structure from the AML stream.
 *
 * A ComputationalData structure is defined as `ComputationalData := ByteConst | WordConst | DWordConst | QWordConst |
 * String | ConstObj | RevisionOp | DefBuffer`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ComputationalData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_computational_data_read(aml_state_t* state, aml_computational_data_t* out)
{
    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_BYTE_PREFIX:
    {
        aml_byte_const_t byte;
        if (aml_byte_const_read(state, &byte) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = byte;
        out->meta.bitWidth = 8;
        return 0;
    }
    case AML_WORD_PREFIX:
    {
        aml_word_const_t word;
        if (aml_word_const_read(state, &word) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = word;
        out->meta.bitWidth = 16;
        return 0;
    }
    case AML_DWORD_PREFIX:
    {
        aml_dword_const_t dword;
        if (aml_dword_const_read(state, &dword) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = dword;
        out->meta.bitWidth = 32;
        return 0;
    }
    case AML_QWORD_PREFIX:
    {
        aml_qword_const_t qword;
        if (aml_qword_const_read(state, &qword) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = qword;
        out->meta.bitWidth = 64;
        return 0;
    }
    case AML_STRING_PREFIX:
        out->type = AML_DATA_STRING;
        return aml_string_read(state, &out->string.content, &out->string.length);
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
    {
        // TODO: Add revision handling
        aml_const_obj_t constObj;
        if (aml_const_obj_read(state, &constObj) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = constObj;
        out->meta.bitWidth = 64;
        return 0;
    }
    case AML_BUFFER_OP:
        out->type = AML_DATA_BUFFER;
        return aml_def_buffer_read(state, &out->buffer.content, &out->buffer.length);
    default:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    }
}

/**
 * @brief Read a DataObject structure from the AML stream.
 *
 * A DataObject structure is defined as `DataObject := ComputationalData | DefPackage | DefVarPackage`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DataObject will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_data_object_read(aml_state_t* state, aml_data_object_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_PACKAGE_OP:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    case AML_VAR_PACKAGE_OP:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    default:
        return aml_computational_data_read(state, out);
    }
}

/**
 * @brief Read a DataRefObject structure from the AML stream.
 *
 * A DataRefObject structure is defined as `DataRefObject := DataObject | ObjectReference`.
 *
 * I have no idea what a ObjectReference is, it is not documented in the ACPI spec except for in the definition of
 * DataRefObject and in the ASL, not AML, section where its defined as an Integer. So for now, we only support
 * DataObject. Insert shrugging emoji here.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DataRefObject will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_data_ref_object_read(aml_state_t* state, aml_data_object_t* out)
{
    return aml_data_object_read(state, out);
}

/** @} */

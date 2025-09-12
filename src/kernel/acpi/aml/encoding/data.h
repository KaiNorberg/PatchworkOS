#pragma once

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"

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

/**
 * @brief ACPI AML ByteData structure.
 */
typedef uint8_t aml_byte_data_t;

/**
 * @brief ACPI AML WordData structure.
 */
typedef uint16_t aml_word_data_t;

/**
 * @brief ACPI AML DWordData structure.
 */
typedef uint32_t aml_dword_data_t;

/**
 * @brief ACPI AML QWordData structure.
 */
typedef uint64_t aml_qword_data_t;

/**
 * @brief The type of the data in a ComputationalData structure.
 */
typedef enum
{
    AML_COMPUTATIONAL_NONE = 0,
    AML_COMPUTATIONAL_BYTE,
    AML_COMPUTATIONAL_WORD,
    AML_COMPUTATIONAL_DWORD,
    AML_COMPUTATIONAL_QWORD,
    AML_COMPUTATIONAL_MAX,
} aml_computational_type_t;

/**
 * @brief ACPI AML ComputationalData structure.
 */
typedef struct
{
    aml_computational_type_t type;
    union {
        aml_byte_data_t byte;
        aml_word_data_t word;
        aml_dword_data_t dword;
        aml_qword_data_t qword;
    };
} aml_computational_data_t;

#define AML_COMPUTATIONAL_DATA_IS_INTEGER(data) \
    ((data).type == AML_COMPUTATIONAL_QWORD || (data).type == AML_COMPUTATIONAL_DWORD || \
        (data).type == AML_COMPUTATIONAL_WORD || (data).type == AML_COMPUTATIONAL_BYTE)

#define AML_COMPUTATIONAL_DATA_AS_INTEGER(data) \
    ((data).type == AML_COMPUTATIONAL_QWORD \
            ? (data).qword \
            : ((data).type == AML_COMPUTATIONAL_DWORD \
                      ? (data).dword \
                      : ((data).type == AML_COMPUTATIONAL_WORD ? (data).word : (data).byte)))

/**
 * @brief ACPI AML Data Object structure.
 */
typedef struct
{
    bool isComputational;
    union {
        aml_computational_data_t computational;
    };
} aml_data_object_t;

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
static inline uint64_t aml_byte_const_read(aml_state_t* state, aml_byte_data_t* out)
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
static inline uint64_t aml_word_const_read(aml_state_t* state, aml_word_data_t* out)
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
 * A DWordConst structure is defined as `DwordConst := DwordC DWordPrefix DWordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_dword_const_read(aml_state_t* state, aml_dword_data_t* out)
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
static inline uint64_t aml_qword_const_read(aml_state_t* state, aml_qword_data_t* out)
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
 * @brief ACPI AML ConstObj structure.
 */
typedef uint64_t aml_const_obj_t;

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
 * @brief Read a ComputationalData structure from the AML stream.
 *
 * A ComputationalData structure is defined as `ComputationalData := ByteConst | WordConst | DWordConst | QWordConst | String | ConstObj |
 * RevisionOp | DefBuffer`.
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
        out->type = AML_COMPUTATIONAL_BYTE;
        return aml_byte_const_read(state, &out->byte);
    case AML_WORD_PREFIX:
        out->type = AML_COMPUTATIONAL_WORD;
        return aml_word_const_read(state, &out->word);
    case AML_DWORD_PREFIX:
        out->type = AML_COMPUTATIONAL_DWORD;
        return aml_dword_const_read(state, &out->dword);
    case AML_QWORD_PREFIX:
        out->type = AML_COMPUTATIONAL_QWORD;
        return aml_qword_const_read(state, &out->qword);
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
        // TODO: Add revision handling
        out->type = AML_COMPUTATIONAL_QWORD;
        return aml_const_obj_read(state, &out->qword);
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
        out->isComputational = true;
        return aml_computational_data_read(state, &out->computational);
    }
}

/** @} */

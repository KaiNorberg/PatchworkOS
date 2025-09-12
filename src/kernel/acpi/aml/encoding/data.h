#pragma once

#include "acpi/aml/aml_state.h"

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
 * A ByteData structure is defined as `0x00 - 0xFF`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ByteData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_byte_data_read(aml_state_t* state, aml_byte_data_t* out);

/**
 * @brief Read a WordData structure from the AML stream.
 *
 * A WordData structure is defined as `ByteData[0:7] ByteData[8:15]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the WordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_word_data_read(aml_state_t* state, aml_word_data_t* out);

/**
 * @brief Read a DWordData structure from the AML stream.
 *
 * A DWordData structure is defined as `WordData[0:15] WordData[16:31]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dword_data_read(aml_state_t* state, aml_dword_data_t* out);

/**
 * @brief Read a QWordData structure from the AML stream.
 *
 * A QWordData structure is defined as `DWordData[0:31] DWordData[32:63]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the QWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_qword_data_read(aml_state_t* state, aml_qword_data_t* out);

/**
 * @brief Read a ByteConst structure from the AML stream.
 *
 * A ByteConst structure is defined as `BytePrefix ByteData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ByteData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_byte_const_read(aml_state_t* state, aml_byte_data_t* out);

/**
 * @brief Read a WordConst structure from the AML stream.
 *
 * A WordConst structure is defined as `WordPrefix WordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the WordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_word_const_read(aml_state_t* state, aml_word_data_t* out);

/**
 * @brief Read a DWordConst structure from the AML stream.
 *
 * A DWordConst structure is defined as `DWordPrefix DWordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dword_const_read(aml_state_t* state, aml_dword_data_t* out);

/**
 * @brief Read a QWordConst structure from the AML stream.
 *
 * A QWordConst structure is defined as `QWordPrefix QWordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the QWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_qword_const_read(aml_state_t* state, aml_qword_data_t* out);

/**
 * @brief Read a ComputationalData structure from the AML stream.
 *
 * A ComputationalData structure is defined as `ByteConst | WordConst | DWordConst | QWordConst | String | ConstObj |
 * RevisionOp | DefBuffer`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ComputationalData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_computational_data_read(aml_state_t* state, aml_computational_data_t* out);

/**
 * @brief Read a DataObject structure from the AML stream.
 *
 * A DataObject structure is defined as `ComputationalData | DefPackage | DefVarPackage`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DataObject will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_read(aml_state_t* state, aml_data_object_t* out);

/** @} */

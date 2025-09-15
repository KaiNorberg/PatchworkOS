#pragma once

#include "acpi/aml/aml_state.h"
#include "name.h"

#include <stdint.h>

typedef struct aml_data_object aml_data_object_t;

/**
 * @brief ACPI AML Data Objects Encoding
 * @defgroup kernel_acpi_aml_data Data Objects
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.3 of the ACPI specification for more details.
 *
 * @{
 */

#include "data_integers.h"

/**
 * @brief ACPI AML String structure.
 */
typedef struct
{
    char* content;
    uint64_t length;
} aml_string_t;

/**
 * @brief ACPI AML Buffer structure.
 */
typedef struct
{
    uint8_t* content;
    uint64_t length;
} aml_buffer_t;

/**
 * @brief ACPI AML NumElements structure.
 */
typedef aml_byte_data_t aml_num_elements_t;

/**
 * @brief ACPI AML Package structure.
 */
typedef struct
{
    aml_data_object_t* elements;
    aml_num_elements_t numElements;
} aml_package_t;

/**
 * @brief ACPI AML DataObject types.
 */
typedef enum
{
    AML_DATA_NONE = 0,
    AML_DATA_INTEGER,
    AML_DATA_STRING,
    AML_DATA_BUFFER,
    AML_DATA_PACKAGE,
    AML_DATA_NAME_STRING,
    AML_DATA_MAX,
} aml_data_type_t;

/**
 * @brief ACPI AML DataObject structure.
 *
 * Represents the DataObject structure found in the specification, but also used to store any generic
 * data in AML, for example the result of a TermArg evaluation or a PackageElement.
 */
typedef struct aml_data_object
{
    aml_data_type_t type;
    union {
        aml_qword_data_t integer;
        aml_string_t string;
        aml_buffer_t buffer;
        aml_package_t package;
        aml_name_string_t nameString;
    };
    struct
    {
        uint8_t bitWidth; // The number of bits of the integer (for INTEGER type only)
    } meta;
} aml_data_object_t;

/**
 * @brief ACPI AML ComputationalData structure.
 */
typedef aml_data_object_t aml_computational_data_t;

/**
 * @brief ACPI AML PackageElement structure.
 */
typedef aml_data_object_t aml_package_element_t;

/**
 * @brief Read a ByteData structure from the AML stream.
 *
 * A ByteData structure is defined as `ByteData := 0x00 - 0xFF`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ByteData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_byte_data_read(aml_state_t* state, aml_byte_data_t* out);

/**
 * @brief Read a WordData structure from the AML stream.
 *
 * A WordData structure is defined as `WordData := ByteData[0:7] ByteData[8:15]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the WordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_word_data_read(aml_state_t* state, aml_word_data_t* out);

/**
 * @brief Read a DWordData structure from the AML stream.
 *
 * A DWordData structure is defined as `DWordData := WordData[0:15] WordData[16:31]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dword_data_read(aml_state_t* state, aml_dword_data_t* out);

/**
 * @brief Read a QWordData structure from the AML stream.
 *
 * A QWordData structure is defined as `QWordData := DWordData[0:31] DWordData[32:63]`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the QWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_qword_data_read(aml_state_t* state, aml_qword_data_t* out);

/**
 * @brief Read a ByteConst structure from the AML stream.
 *
 * A ByteConst structure is defined as `ByteConst := BytePrefix ByteData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the ByteData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_byte_const_read(aml_state_t* state, aml_byte_const_t* out);

/**
 * @brief Read a WordConst structure from the AML stream.
 *
 * A WordConst structure is defined as `WordConst := WordPrefix WordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the WordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_word_const_read(aml_state_t* state, aml_word_const_t* out);

/**
 * @brief Read a DWordConst structure from the AML stream.
 *
 * A DWordConst structure is defined as `DwordConst := DWordPrefix DWordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dword_const_read(aml_state_t* state, aml_dword_const_t* out);

/**
 * @brief Read a QWordConst structure from the AML stream.
 *
 * A QWordConst structure is defined as `QWordConst := QWordPrefix QWordData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the QWordData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_qword_const_read(aml_state_t* state, aml_qword_const_t* out);

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
uint64_t aml_const_obj_read(aml_state_t* state, aml_const_obj_t* out);

/**
 * @brief Read a String structure from the AML stream.
 *
 * A String structure is defined as `String := StringPrefix AsciiCharList NullChar`.
 *
 * AsciiCharList is defined as a sequence of ASCII characters in the range 0x01 to 0x7F, and NullChar is defined as
 * 0x00.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the String will be stored. This will point to a location within the AML
 * bytestream and should not be freed or modified.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_string_read(aml_state_t* state, aml_string_t* out);

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
uint64_t aml_computational_data_read(aml_state_t* state, aml_computational_data_t* out);

/**
 * @brief Read a NumElements structure from the AML stream.
 *
 * A NumElements structure is defined as `NumElements := ByteData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the NumElements will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_num_elements_read(aml_state_t* state, aml_num_elements_t* out);

/**
 * @brief Read a PackageElement structure from the AML stream.
 *
 * A PackageElement structure is defined as `PackageElement := DataRefObject | NameString`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the Package element will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_package_element_read(aml_state_t* state, aml_package_element_t* out);

/**
 * @brief Read a PackageElementList structure from the AML stream.
 *
 * A PackageElementList structure is defined as PackageElementList := Nothing | <packageelement packageelementlist>`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the Package elements will be stored. This will be allocated by this
 * function and should be freed with `aml_package_free()`.
 * @param numElements The number of elements to read.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_package_element_list_read(aml_state_t* state, aml_package_element_t** out, aml_num_elements_t numElements);

/**
 * @brief Reads a DefPackage structure from the AML byte stream.
 *
 * A DefPackage structure is defined as `DefPackage := PackageOp PkgLength NumElements PackageElementList`.
 *
 * See section 19.6.102 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the Package will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_package_read(aml_state_t* state, aml_package_t* out);

/**
 * @brief Read a DataObject structure from the AML stream.
 *
 * A DataObject structure is defined as `DataObject := ComputationalData | DefPackage | DefVarPackage`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DataObject will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_read(aml_state_t* state, aml_data_object_t* out);

/**
 * @brief Read a DataRefObject structure from the AML stream.
 *
 * A DataRefObject structure is defined as `DataRefObject := DataObject | ObjectReference`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the DataRefObject will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_ref_object_read(aml_state_t* state, aml_data_object_t* out);

/**
 * @brief Frees the memory allocated for a Package structure.
 */
void aml_package_free(aml_package_t* package);

/** @} */

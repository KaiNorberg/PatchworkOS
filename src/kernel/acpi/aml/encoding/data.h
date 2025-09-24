#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct aml_state aml_state_t;
typedef struct aml_node aml_node_t;

/**
 * @brief ACPI AML Data Objects Encoding
 * @defgroup kernel_acpi_aml_data Data Objects
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.3 of the ACPI specification for more details.
 *
 * @{
 */

#include "data_integers.h"

/**
 * @brief ACPI AML String structure.
 * @struct aml_string_t
 */
typedef struct
{
    char* content;
    uint64_t length;
    bool inPlace;
} aml_string_t;

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
 * @see Sections 19.6.98, 19.6.99 and 19.6.156 for more details.
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
 * @param node The current AML node.
 * @param out Pointer to the node where the ComputationalData will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_computational_data_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Read a NumElements structure from the AML stream.
 *
 * A NumElements structure is defined as `NumElements := ByteData`.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the NumElements will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_num_elements_read(aml_state_t* state, aml_byte_data_t* out);

/**
 * @brief Read a PackageElement structure from the AML stream.
 *
 * A PackageElement structure is defined as `PackageElement := DataRefObject | NameString`.
 *
 * @see Section 19.6.102 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the PackageElement will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_package_element_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Read a PackageElementList structure from the AML stream.
 *
 * A PackageElementList structure is defined as PackageElementList := Nothing | <packageelement packageelementlist>`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param package Pointer to the Package node to be filled with the elements.
 * @param end The address in the AML stream where the PackageElementList ends.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_package_element_list_read(aml_state_t* state, aml_node_t* node, aml_node_t* package, aml_address_t end);

/**
 * @brief Reads a DefPackage structure from the AML byte stream.
 *
 * A DefPackage structure is defined as `DefPackage := PackageOp PkgLength NumElements PackageElementList`.
 *
 * @see Section 19.6.102 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the Package will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_package_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Read a DataObject structure from the AML stream.
 *
 * A DataObject structure is defined as `DataObject := ComputationalData | DefPackage | DefVarPackage`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the DataObject will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Read a DataRefObject structure from the AML stream.
 *
 * A DataRefObject structure is defined as `DataRefObject := DataObject | ObjectReference`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the DataRefObject will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_ref_object_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/** @} */

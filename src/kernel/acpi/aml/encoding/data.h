#pragma once

#include "acpi/aml/aml_integer.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct aml_object aml_object_t;
typedef struct aml_package_obj aml_package_obj_t;
typedef struct aml_term_list_ctx aml_term_list_ctx_t;

/**
 * @brief Data Objects Encoding
 * @defgroup kernel_acpi_aml_encoding_data Data Objects
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.3 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Read a ByteData structure from the AML stream.
 *
 * A ByteData structure is defined as `ByteData := 0x00 - 0xFF`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer where the byte value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_byte_data_read(aml_term_list_ctx_t* ctx, uint8_t* out);

/**
 * @brief Read a WordData structure from the AML stream.
 *
 * A WordData structure is defined as `WordData := ByteData[0:7] ByteData[8:15]`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer where the word value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_word_data_read(aml_term_list_ctx_t* ctx, uint16_t* out);

/**
 * @brief Read a DWordData structure from the AML stream.
 *
 * A DWordData structure is defined as `DWordData := WordData[0:15] WordData[16:31]`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer where the dword value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dword_data_read(aml_term_list_ctx_t* ctx, uint32_t* out);

/**
 * @brief Read a QWordData structure from the AML stream.
 *
 * A QWordData structure is defined as `QWordData := DWordData[0:31] DWordData[32:63]`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the qword value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_qword_data_read(aml_term_list_ctx_t* ctx, uint64_t* out);

/**
 * @brief Read a ByteConst structure from the AML stream.
 *
 * A ByteConst structure is defined as `ByteConst := BytePrefix ByteData`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the byte value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_byte_const_read(aml_term_list_ctx_t* ctx, uint8_t* out);

/**
 * @brief Read a WordConst structure from the AML stream.
 *
 * A WordConst structure is defined as `WordConst := WordPrefix WordData`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the word value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_word_const_read(aml_term_list_ctx_t* ctx, uint16_t* out);

/**
 * @brief Read a DWordConst structure from the AML stream.
 *
 * A DWordConst structure is defined as `DwordConst := DWordPrefix DWordData`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the dword value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dword_const_read(aml_term_list_ctx_t* ctx, uint32_t* out);

/**
 * @brief Read a QWordConst structure from the AML stream.
 *
 * A QWordConst structure is defined as `QWordConst := QWordPrefix QWordData`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the qword value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_qword_const_read(aml_term_list_ctx_t* ctx, uint64_t* out);

/**
 * @brief Read a ConstObj structure from the AML stream.
 *
 * A ConstObj structure is defined as `ConstObj := ZeroOp | OneOp | OnesOp`.
 *
 * @see Sections 19.6.98, 19.6.99 and 19.6.156 for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the object to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_const_obj_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Read a String structure from the AML stream.
 *
 * A String structure is defined as `String := StringPrefix AsciiCharList NullChar`.
 *
 * AsciiCharList is defined as a sequence of ASCII characters in the range 0x01 to 0x7F, and NullChar is defined as
 * 0x00.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the object to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_string_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Read a RevisionOp structure from the AML stream.
 *
 * A RevisionOp structure is defined as `RevisionOp := RevisionOp`.
 *
 * @see Section 19.6.121 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the object to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_revision_op_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Read a ComputationalData structure from the AML stream.
 *
 * A ComputationalData structure is defined as `ComputationalData := ByteConst | WordConst | DWordConst | QWordConst |
 * String | ConstObj | RevisionOp | DefBuffer`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the object to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_computational_data_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Read a NumElements structure from the AML stream.
 *
 * A NumElements structure is defined as `NumElements := ByteData`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the integer to be filled with the number of elements.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_num_elements_read(aml_term_list_ctx_t* ctx, uint8_t* out);

/**
 * @brief Read a PackageElement structure from the AML stream.
 *
 * A PackageElement structure is defined as `PackageElement := DataRefObject | NameString`.
 *
 * @see Section 19.6.102 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to the object to initialize with the read element.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_package_element_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Read a PackageElementList structure from the AML stream.
 *
 * A PackageElementList structure is defined as PackageElementList := Nothing | <packageelement packageelementlist>`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param package Pointer to the package to fill with the read elements.
 * @param end Pointer to the end of the PackageElementList.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_package_element_list_read(aml_term_list_ctx_t* ctx, aml_package_obj_t* package,
    const uint8_t* end);

/**
 * @brief Reads a DefPackage structure from the AML byte stream.
 *
 * A DefPackage structure is defined as `DefPackage := PackageOp PkgLength NumElements PackageElementList`.
 *
 * @see Section 19.6.102 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the object to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_package_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Read a VarNumElements structure from the AML stream.
 *
 * A VarNumElements structure is defined as `VarNumElements := TermArg => Integer`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the integer to be filled with the number of elements.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_var_num_elements_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Reads a DefVarPackage structure from the AML byte stream.
 *
 * A DefVarPackage structure is defined as `DefVarPackage := VarPackageOp PkgLength VarNumElements PackageElementList`.
 *
 * @see Section 19.6.103 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the object to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_var_package_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Read a DataObject structure from the AML stream.
 *
 * A DataObject structure is defined as `DataObject := ComputationalData | DefPackage | DefVarPackage`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the object to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Read a DataRefObject structure from the AML stream.
 *
 * A DataRefObject structure is defined as `DataRefObject := DataObject | ObjectReference`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Output pointer to the object to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_ref_object_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/** @} */

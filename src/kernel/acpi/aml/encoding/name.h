#pragma once

#include "data_integers.h"

#include <stdint.h>
#include <sys/list.h>

typedef struct aml_state aml_state_t;
typedef struct aml_node aml_node_t;
typedef struct aml_data_object aml_data_object_t;
typedef struct aml_object_reference aml_object_reference_t;

/**
 * @brief ACPI AML Name Objects Encoding
 * @defgroup kernel_acpi_aml_name Name Objects
 * @ingroup kernel_acpi_aml
 *
 * Not to be confused with "ACPI AML Named Objects Encoding".
 *
 * @see Section 20.2.2 of the ACPI specification for more details.
 *
 * @{
 */

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
 * @brief ACPI AML SegCount structure.
 */
typedef aml_byte_data_t aml_seg_count_t;

/**
 * @brief A NameSeg strcture.
 * @struct aml_name_seg_t
 */
typedef struct
{
    char name[AML_NAME_LENGTH];
} aml_name_seg_t;

/**
 * @brief Represents the NamePath, DualNamePath, MultiNamePath and NullPath structures.
 * @struct aml_name_path_t
 */
typedef struct
{
    aml_seg_count_t segmentCount; //!< Number of segments in the name path.
    aml_name_seg_t* segments;     //!< Array of segments in the name path.
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
 * @brief Reads the next data as a SegCount structure from the AML bytecode stream.
 *
 * A SegCount structure is defined as `SegCount := ByteData`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the SegCount will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_seg_count_read(aml_state_t* state, aml_seg_count_t* out);

/**
 * @brief Reads the next data as a NameSeg from the AML bytecode stream.
 *
 * A NameSeg structure is defined as `NameSeg := <leadnamechar namechar namechar namechar>`.
 *
 * @param state The AML state.
 * @param out Pointer to the destination where the pointer to the NameSeg will be stored. Will be located within the AML
 * bytecode stream.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_name_seg_read(aml_state_t* state, aml_name_seg_t** out);

/**
 * @brief Reads the next data as a DualNamePath structure from the AML bytecode stream.
 *
 * A DualNamePath structure is defined as `DualNamePath := DualNamePrefix NameSeg NameSeg`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the pointer to the array of two NameSeg will be stored. Will be located
 * within the AML bytecode stream.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_dual_name_path_read(aml_state_t* state, aml_name_seg_t** out);

/**
 * @brief Reads the next data as a MultiNamePath structure from the AML bytecode stream.
 *
 * A MultiNamePath structure is defined as `MultiNamePath := MultiNamePrefix SegCount NameSeg(SegCount)`.
 *
 * @param state The AML state.
 * @param outSegments Pointer to destination where the pointer to the array of NameSeg will be stored. Will be located
 * within the AML bytecode stream.
 * @param outSegCount Pointer to destination where the number of segments will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_multi_name_path_read(aml_state_t* state, aml_name_seg_t** outSegments, aml_seg_count_t* outSegCount);

/**
 * Reads the next data as a NullName structure from the AML bytecode stream.
 *
 * A NullName structure is defined as `NullName := 0x00`.
 *
 * @param state The AML state.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_null_name_read(aml_state_t* state);

/**
 * @brief Reads the next data as a NamePath structure from the AML bytecode stream.
 *
 * A NamePath structure is defined as `NamePath := NameSeg | DualNamePath | MultiNamePath | NullName`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the NamePath will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_name_path_read(aml_state_t* state, aml_name_path_t* out);

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
uint64_t aml_prefix_path_read(aml_state_t* state, aml_prefix_path_t* out);

/**
 * @brief Reads the next data as a RootChar from the AML bytecode stream.
 *
 * A RootChar is defined as `RootChar := 0x5C`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the RootChar will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_root_char_read(aml_state_t* state, aml_root_char_t* out);

/**
 * @brief Reads the next data as a NameString structure from the AML bytecode stream.
 *
 * A NameString structure is defined as `NameString := <rootchar namepath> | <prefixpath namepath>`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the name string will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_name_string_read(aml_state_t* state, aml_name_string_t* out);

/**
 * @brief Reads a SimpleName structure from the AML byte stream.
 *
 * A SimpleName structure is defined as `SimpleName := NameString | ArgObj | LocalObj`.
 *
 * If the SimpleName does not resolve to an object, then out->type == AML_OBJECT_REFERENCE_EMPTY.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the object reference will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_simple_name_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out);

/**
 * @brief Reads a SuperName structure from the AML byte stream.
 *
 * A SuperName structure is defined as `SuperName := SimpleName | DebugObj | ReferenceTypeOpcode`.
 *
 * If the SuperName does not resolve to an object, then out->type == AML_OBJECT_REFERENCE_EMPTY.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the object reference will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_super_name_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out);

/**
 * @brief Reads a Target structure from the AML byte stream.
 *
 * A Target structure is defined as `Target := SuperName | NullName`.
 *
 * If the Target is a NullName or the SuperName does not resolve to an object, then out->type == AML_OBJECT_REFERENCE_EMPTY.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the object reference will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_target_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out);

/** @} */

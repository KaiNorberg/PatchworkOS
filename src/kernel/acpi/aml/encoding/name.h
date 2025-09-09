#pragma once

#include "acpi/aml/aml_state.h"

#include <stdint.h>

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

#define AML_MAX_NAME_PATH 254
#define AML_MAX_NAME_SEG 4

#define AML_IS_LEAD_NAME_CHAR(c) ((c >= 'A' && c <= 'Z') || c == '_')
#define AML_IS_DIGIT_CHAR(c) ((c >= '0' && c <= '9'))
#define AML_IS_NAME_CHAR(c) (AML_IS_DIGIT_CHAR(c) || AML_IS_LEAD_NAME_CHAR(c))

#define AML_ROOT_CHAR '\\'
#define AML_PARENT_PREFIX_CHAR '^'
#define AML_DUAL_NAME_PREFIX 0x2E
#define AML_MULTI_NAME_PREFIX 0x2F
#define AML_NULL_NAME 0x00

/**
 * @brief A PrefixPath structure.
 * @struct aml_prefix_path_t
 */
typedef struct
{
    uint16_t depth; //!< Number of parent prefixes ('^') in the prefix, each prefix means go back one level in the
                    //!< namespace hierarchy.
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
    uint8_t name[AML_MAX_NAME_SEG];
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
 * @brief Reads the next data as a RootChar from the AML bytecode stream.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the RootChar will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_root_char_read(aml_state_t* state, aml_root_char_t* out);

/**
 * @brief Reads the next data as a NameSeg from the AML bytecode stream.
 *
 * A NameSeg structure is defined as `<leadnamechar namechar namechar namechar>`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the NameSeg will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_name_seg_read(aml_state_t* state, aml_name_seg_t* out);

/**
 * @brief Reads the next data as a DualNamePath structure from the AML bytecode stream.
 *
 * A DualNamePath structure is defined as `DualNamePrefix NameSeg NameSeg`.
 *
 * @param state The AML state.
 * @param firstOut Pointer to destination where the first segment of the DualNamePath will be stored.
 * @param secondOut Pointer to destination where the second segment of the DualNamePath will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_dual_name_path_read(aml_state_t* state, aml_name_seg_t* firstOut, aml_name_seg_t* secondOut);

/**
 * @brief Reads the next data as a MultiNamePath structure from the AML bytecode stream.
 *
 * A MultiNamePath structure is defined as `MultiNamePrefix SegCount NameSeg(SegCount)`.
 *
 * @param state The AML state.
 * @param outSegments Pointer to destination where the segments of the MultiNamePath will be stored.
 * @param outSegCount Pointer to destination where the number of segments will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_multi_name_path_read(aml_state_t* state, aml_name_seg_t* outSegments, uint8_t* outSegCount);

/**
 * Reads the next data as a NullName structure from the AML bytecode stream.
 *
 * A NullName structure is defined as `0x00`.
 *
 * @param state The AML state.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_null_name_read(aml_state_t* state);

/**
 * @brief Reads the next data as a NamePath structure from the AML bytecode stream.
 *
 * A NamePath structure is defined as `NameSeg | DualNamePath | MultiNamePath | NullName`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the NamePath will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_name_path_read(aml_state_t* state, aml_name_path_t* out);

/**
 * @brief Reads the next data as a PrefixPath structure from the AML bytecode stream.
 *
 * A PrefixPath structure is defined as `Nothing | <'^' prefixpath>`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the PrefixPath will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_prefix_path_read(aml_state_t* state, aml_prefix_path_t* out);

/**
 * @brief Reads the next data as a NameString structure from the AML bytecode stream.
 *
 * A NameString structure is defined as `<rootchar namepath> | <prefixpath namepath>`.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the name string will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_name_string_read(aml_state_t* state, aml_name_string_t* out);

/**
 * @brief Walks the ACPI namespace tree to find the node corresponding to the given NameString.
 *
 * @param nameString The NameString to search for.
 * @param start The node to start the search from, or `NULL` to start from the root.
 * @return On success, a pointer to the found node. On error, `NULL` and `errno` is set.
 */
aml_node_t* aml_name_string_walk(const aml_name_string_t* nameString, aml_node_t* start);

/** @} */

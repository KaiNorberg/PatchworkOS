#pragma once

#include <stdint.h>
#include <sys/list.h>

typedef struct aml_term_list_ctx aml_term_list_ctx_t;
typedef struct aml_object aml_object_t;
typedef struct aml_term_list_ctx aml_term_list_ctx_t;

/**
 * @brief Name Objects Encoding
 * @defgroup kernel_acpi_aml_encoding_name Name Objects
 * @ingroup kernel_acpi_aml
 *
 * Not to be confused with "ACPI AML Named Objects Encoding".
 *
 * @see Section 20.2.2 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Check if a token is a LeadNameChar structure.
 *
 * @param token The token to check.
 * @return true if the token is a LeadNameChar structure, false otherwise.
 */
#define AML_IS_LEAD_NAME_CHAR(token) \
    (((token)->num >= AML_NAME_CHAR_A && (token)->num <= AML_NAME_CHAR_Z) || (token)->num == AML_NAME_CHAR)

/**
 * @brief Check if a token is a DigitChar structure.
 *
 * @param token The token to check.
 * @return true if the token is a DigitChar structure, false otherwise.
 */
#define AML_IS_DIGIT_CHAR(token) (((token)->num >= AML_DIGIT_CHAR_0 && (token)->num <= AML_DIGIT_CHAR_9))

/**
 * @brief Check if a token is a NameChar structure.
 *
 * @param token The token to check.
 * @return true if the token is a NameChar structure, false otherwise.
 */
#define AML_IS_NAME_CHAR(token) (AML_IS_DIGIT_CHAR(token) || AML_IS_LEAD_NAME_CHAR(token))

/**
 * @brief A PrefixPath structure.
 * @struct aml_prefix_path_t
 */
typedef struct
{
    uint16_t depth; ///< Number of parent prefixes ('^') in the prefix, each prefix means go back one level in the
                    /// namespace hierarchy.
} aml_prefix_path_t;

/**
 * @brief A RootChar structure.
 * @struct aml_root_char_t
 */
typedef struct
{
    bool present; ///< If the first character is a root character ('\\'), if yes, the name string is absolute.
} aml_root_char_t;

/**
 * @brief A NameSeg strcture.
 * @struct aml_name_seg_t
 */
typedef uint32_t aml_name_seg_t;

/**
 * @brief Represents the NamePath, DualNamePath, MultiNamePath and NullPath structures.
 * @struct aml_name_path_t
 */
typedef struct
{
    uint64_t segmentCount;    ///< Number of segments in the name path.
    aml_name_seg_t* segments; ///< Array of segments in the name path.
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
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to destination where the SegCount will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_seg_count_read(aml_term_list_ctx_t* ctx, uint8_t* out);

/**
 * @brief Reads the next data as a NameSeg from the AML bytecode stream.
 *
 * A NameSeg structure is defined as `NameSeg := <leadnamechar namechar namechar namechar>`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to the destination where the pointer to the NameSeg will be stored. Will be located within the AML
 * bytecode stream.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_name_seg_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** out);

/**
 * @brief Reads the next data as a DualNamePath structure from the AML bytecode stream.
 *
 * A DualNamePath structure is defined as `DualNamePath := DualNamePrefix NameSeg NameSeg`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to destination where the pointer to the array of two NameSeg will be stored. Will be located
 * within the AML bytecode stream.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dual_name_path_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** out);

/**
 * @brief Reads the next data as a MultiNamePath structure from the AML bytecode stream.
 *
 * A MultiNamePath structure is defined as `MultiNamePath := MultiNamePrefix SegCount NameSeg(SegCount)`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param outSegments Pointer to destination where the pointer to the array of NameSeg will be stored. Will be located
 * within the AML bytecode stream.
 * @param outSegCount Pointer to destination where the number of segments will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_multi_name_path_read(aml_term_list_ctx_t* ctx, aml_name_seg_t** outSegments, uint64_t* outSegCount);

/**
 * Reads the next data as a NullName structure from the AML bytecode stream.
 *
 * A NullName structure is defined as `NullName := 0x00`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_null_name_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads the next data as a NamePath structure from the AML bytecode stream.
 *
 * A NamePath structure is defined as `NamePath := NameSeg | DualNamePath | MultiNamePath | NullName`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to destination where the NamePath will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_name_path_read(aml_term_list_ctx_t* ctx, aml_name_path_t* out);

/**
 * @brief Reads the next data as a PrefixPath structure from the AML bytecode stream.
 *
 * A PrefixPath structure is defined as `PrefixPath := Nothing | <'^' prefixpath>`.
 *
 * Note that `^` is just a `AML_PARENT_PREFIX_CHAR`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to destination where the PrefixPath will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_prefix_path_read(aml_term_list_ctx_t* ctx, aml_prefix_path_t* out);

/**
 * @brief Reads the next data as a RootChar from the AML bytecode stream.
 *
 * A RootChar is defined as `RootChar := 0x5C`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to destination where the RootChar will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_root_char_read(aml_term_list_ctx_t* ctx, aml_root_char_t* out);

/**
 * @brief Reads the next data as a NameString structure from the AML bytecode stream.
 *
 * A NameString structure is defined as `NameString := <rootchar namepath> | <prefixpath namepath>`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to destination where the NameString will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_name_string_read(aml_term_list_ctx_t* ctx, aml_name_string_t* out);

/**
 * @brief Reads the next data as a NameString structure from the AML bytecode stream and resolves it to a object.
 *
 * Note that `errno` will only be set to `ENOENT` if the NameString is read correctly but fails to resolve, other values
 * for `errno` might be set in other cases.
 *
 * If the name string points to a non-existing object, a integer object containing `0` will be created and returned.
 * This is as always for the sake of compatibility, even if the specification does not specify this behavior.
 *
 * @see Section 5.3 of the ACPI specification for more details.
 * @see aml_name_string_find_by_name_string() for details on how the resolution is performed.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, a pointer to the resolved object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_name_string_read_and_resolve(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a SimpleName structure from the AML byte stream and resolves it to a object.
 *
 * A SimpleName structure is defined as `SimpleName := NameString | ArgObj | LocalObj`.
 *
 * Note that `errno` will only be set to `ENOENT` if it is a NameString that fails to resolve, other values for `errno`
 * might be set in other cases.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, a pointer to the resolved object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_simple_name_read_and_resolve(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a SuperName structure from the AML byte stream and resolves it to a object.
 *
 * A SuperName structure is defined as `SuperName := SimpleName | DebugObj | ReferenceTypeOpcode`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, a pointer to the resolved object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_super_name_read_and_resolve(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a Target structure from the AML byte stream and resolves it to a object.
 *
 * A Target structure is defined as `Target := SuperName | NullName`.
 *
 * If the Target is a NullName, then out will be set to point to `NULL` but its not considered an error.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param out Pointer to where the pointer to the resolved object will be stored, might be set to point to `NULL`.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_target_read_and_resolve(aml_term_list_ctx_t* ctx, aml_object_t** out);

/** @} */

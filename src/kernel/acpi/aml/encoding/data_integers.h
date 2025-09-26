#pragma once

#include <stdint.h>

/**
 * @addtogroup kernel_acpi_aml_data
 *
 * @{
 */

/**
 * @brief The bit width of an AML integer.
 *
 * This is technically decided by the Definition Blocks revision field but we only support revision 2 and up which
 * means 64 bits.
 */
#define AML_INTEGER_BIT_WIDTH 64

/**
 * @brief Represents a size in bits within an opregion.
 */
typedef uint64_t aml_bit_size_t;

/**
 * @brief ByteData structure.
 * @typedef aml_byte_data_t
 */
typedef uint8_t aml_byte_data_t;

/**
 * @brief WordData structure.
 * @typedef aml_word_data_t
 */
typedef uint16_t aml_word_data_t;

/**
 * @brief DWordData structure.
 * @typedef aml_dword_data_t
 */
typedef uint32_t aml_dword_data_t;

/**
 * @brief QWordData structure.
 * @typedef aml_qword_data_t
 */
typedef uint64_t aml_qword_data_t;

/**
 * @brief ByteConst structure.
 * @typedef aml_byte_const_t
 */
typedef aml_byte_data_t aml_byte_const_t;

/**
 * @brief WordConst structure.
 * @typedef aml_word_const_t
 */
typedef aml_word_data_t aml_word_const_t;

/**
 * @brief DWordConst structure.
 * @typedef aml_dword_const_t
 */
typedef aml_dword_data_t aml_dword_const_t;

/**
 * @brief QWordConst structure.
 * @typedef aml_qword_const_t
 */
typedef aml_qword_data_t aml_qword_const_t;

/**
 * @brief ConstObj structure.
 * @typedef aml_const_obj_t
 */
typedef aml_qword_data_t aml_const_obj_t;

/** @} */

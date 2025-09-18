#pragma once

#include <stdint.h>

/**
 * @addtogroup kernel_acpi_aml_data
 *
 * @{
 */

/**
 * @brief Represents a size in bits within an opregion.
 */
typedef uint64_t aml_bit_size_t;

/**
 * @brief ACPI AML ByteData structure.
 * @typedef aml_byte_data_t
 */
typedef uint8_t aml_byte_data_t;

/**
 * @brief ACPI AML WordData structure.
 * @typedef aml_word_data_t
 */
typedef uint16_t aml_word_data_t;

/**
 * @brief ACPI AML DWordData structure.
 * @typedef aml_dword_data_t
 */
typedef uint32_t aml_dword_data_t;

/**
 * @brief ACPI AML QWordData structure.
 * @typedef aml_qword_data_t
 */
typedef uint64_t aml_qword_data_t;

/**
 * @brief ACPI AML ByteConst structure.
 * @typedef aml_byte_const_t
 */
typedef aml_byte_data_t aml_byte_const_t;

/**
 * @brief ACPI AML WordConst structure.
 * @typedef aml_word_const_t
 */
typedef aml_word_data_t aml_word_const_t;

/**
 * @brief ACPI AML DWordConst structure.
 * @typedef aml_dword_const_t
 */
typedef aml_dword_data_t aml_dword_const_t;

/**
 * @brief ACPI AML QWordConst structure.
 * @typedef aml_qword_const_t
 */
typedef aml_qword_data_t aml_qword_const_t;

/**
 * @brief ACPI AML ConstObj structure.
 * @typedef aml_const_obj_t
 */
typedef aml_qword_data_t aml_const_obj_t;

/** @} */

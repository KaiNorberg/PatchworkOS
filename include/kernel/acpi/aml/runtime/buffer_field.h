#pragma once

#include <kernel/acpi/aml/object.h>

#include <stdint.h>

/**
 * @brief Buffer Field
 * @defgroup kernel_acpi_aml_buffer_field Buffer Field
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Read the value stored in a BufferField and store it in the out object.
 *
 * A BufferField is a view into a buffer, it has a bit offset and a bit size, and can be used to read or write parts of
 * the buffer.
 *
 * @see @ref kernel_acpi_aml_evaluate
 *
 * @param bufferField The buffer field to read from.
 * @param out Pointer to the buffer where the result will be stored, will be an integer or a buffer.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_field_load(aml_buffer_field_t* bufferField, aml_object_t* out);

/**
 * @brief Write a value to a BufferField.
 *
 * A BufferField is a view into a buffer, it has a bit offset and a bit size, and can be used to read or write parts of
 * the buffer.
 *
 * @see @ref kernel_acpi_aml_evaluate
 *
 * @param bufferField The buffer field to write to.
 * @param in Pointer to the object containing the value to write, must be an integer or a buffer.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_field_store(aml_buffer_field_t* bufferField, aml_object_t* in);

/** @} */

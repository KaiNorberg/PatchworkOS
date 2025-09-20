#pragma once

#include "mem/heap.h"
#include "name.h"
#include "object_reference.h"

#include <stdint.h>

typedef struct aml_data_object aml_data_object_t;

/**
 * @addtogroup kernel_acpi_aml_data
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
    bool allocated; // Whether the content was allocated (and should be freed) or not.
} aml_string_t;

/**
 * @brief Macro to create a string structure in place, without allocating memory for the content.
 *
 * @param string Pointer to the string content.
 * @param len Length of the string (excluding null-terminator).
 * @return aml_string_t The created string structure.
 */
#define AML_STRING_CREATE_IN_PLACE(string, len) \
    (aml_string_t) \
    { \
        .content = string, .length = len, .allocated = false, \
    }

/**
 * @brief ACPI AML Buffer structure.
 * @struct aml_buffer_t
 */
typedef struct
{
    uint8_t* content;  // Pointer to the buffer content.
    uint64_t length;   // The current length of the buffer.
    uint64_t capacity; // The allocated capacity of the buffer.
    bool allocated;    // Whether the content was allocated (and should be freed) or not.
} aml_buffer_t;

/**
 * @brief Macro to create a buffer structure in place, allocating memory for the content.
 *
 * @param bufCap Capacity of the buffer.
 * @return aml_buffer_t The created buffer structure.
 */
#define AML_BUFFER_CREATE(bufCap) \
    (aml_buffer_t) \
    { \
        .content = heap_alloc(bufCap, HEAP_NONE), .length = 0, .capacity = bufCap, .allocated = true, \
    }

/**
 * @brief Macro to create a buffer structure in place, without allocating memory for the content.
 *
 * @param buffer Pointer to the buffer content.
 * @param len Length of the buffer.
 * @return aml_buffer_t The created buffer structure.
 */
#define AML_BUFFER_CREATE_IN_PLACE(buffer, len) \
    (aml_buffer_t) \
    { \
        .content = buffer, .length = len, .capacity = len, .allocated = false, \
    }

/**
 * @brief ACPI AML NumElements structure.
 * @typedef aml_num_elements_t
 */
typedef aml_byte_data_t aml_num_elements_t;

/**
 * @brief ACPI AML Package structure.
 * @struct aml_package_t
 */
typedef struct
{
    aml_data_object_t* elements;
    aml_num_elements_t numElements;
} aml_package_t;

/**
 * @brief Macro to create a package structure in place, without allocating memory for the elements.
 *
 * @param elems Pointer to the package elements.
 * @param num Number of elements in the package.
 * @return aml_package_t The created package structure.
 */
#define AML_PACKAGE_CREATE_IN_PLACE(elems, num) \
    (aml_package_t) \
    { \
        .elements = elems, .numElements = num, \
    }

/**
 * @brief ACPI AML DataObject types.
 * @enum aml_data_type_t
 */
typedef enum
{
    AML_DATA_NONE = 0,
    AML_DATA_INTEGER,
    AML_DATA_STRING,
    AML_DATA_BUFFER,
    AML_DATA_PACKAGE,
    AML_DATA_NAME_STRING,
    AML_DATA_OBJECT_REFERENCE,
    AML_DATA_ANY,
    AML_DATA_MAX,
} aml_data_type_t;

/**
 * @brief ACPI AML DataObject structure.
 * @struct aml_data_object_t
 *
 * Represents the DataObject structure found in the specification, but also used to store any generic
 * data in the AML parser, for example the result of a TermArg evaluation or a PackageElement.
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
        aml_object_reference_t objectReference;
    };
    struct
    {
        uint8_t bitWidth; // The number of bits of the integer (for INTEGER type only)
    } meta;
} aml_data_object_t;

/**
 * @brief Initializes a DataObject as an Integer.
 *
 * @param obj Pointer to the empty DataObject to initialize.
 * @param value The integer value to set.
 * @param bitWidth The bit width of the integer (8, 16, 32, or 64).
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_init_integer(aml_data_object_t* obj, aml_qword_data_t value, uint8_t bitWidth);

/**
 * @brief Initializes a DataObject as a String, copying the provided string structure but not its content.
 *
 * Be careful to avoid duble frees, since the string content is not copied.
 *
 * @param obj Pointer to the empty DataObject to initialize.
 * @param str Pointer to the string content to copy.
 * @param length Length of the string (excluding null-terminator).
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_init_string(aml_data_object_t* obj, aml_string_t* str);

/**
 * @brief Initializes a DataObject as a Buffer, copying the provided buffer structure but not its content.
 *
 * Be careful to avoid duble frees, since the buffer content is not copied.
 *
 * @param obj Pointer to the empty DataObject to initialize.
 * @param data Pointer to the buffer content to copy.
 * @param length Length of the buffer.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_init_buffer(aml_data_object_t* obj, aml_buffer_t* buffer);

/**
 * @brief Initializes a DataObject as a Buffer, allocating an empty buffer of the specified length.
 *
 * @param obj Pointer to the empty DataObject to initialize.
 * @param data Pointer to the buffer content to use in place.
 * @param length Length of the buffer.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_init_buffer_empty(aml_data_object_t* obj, uint64_t length);

/**
 * @brief Initializes a DataObject as a Package, copying the provided Package structure but not its content.
 *
 * Be careful to avoid duble frees, since the package content is not copied.
 *
 * @param obj Pointer to the empty DataObject to initialize.
 * @param numElements Number of elements in the package.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_init_package(aml_data_object_t* obj, aml_package_t* package);

/**
 * @brief Initializes a DataObject as a NameString, copying the provided NameString structure but not its content.
 *
 * @param obj Pointer to the empty DataObject to initialize.
 * @param nameString Pointer to the NameString to copy.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_init_name_string(aml_data_object_t* obj, aml_name_string_t* nameString);

/**
 * @brief Initializes a DataObject as an ObjectReference, copying the provided ObjectReference structure but not its
 * content.
 *
 * @param obj Pointer to the empty DataObject to initialize.
 * @param ref Pointer to the ObjectReference to copy.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_init_object_reference(aml_data_object_t* obj, aml_object_reference_t* ref);

/**
 * @brief Frees the memory allocated for a String structure.
 *
 * Note that this does not free the String structure itself, only its content, and only if it was allocated. Since some
 * strings are stored directly in the AML bytecode.
 *
 * @param string Pointer to the String structure to free.
 */
void aml_string_deinit(aml_string_t* string);

/**
 * @brief Frees the memory allocated for a Buffer structure.
 *
 * Note that this does not free the Buffer structure itself, only its content, and only if it was allocated. Since some
 * buffers are stored directly in the AML bytecode.
 *
 * @param buffer Pointer to the Buffer whose content will be freed.
 */
void aml_buffer_deinit(aml_buffer_t* buffer);

/**
 * @brief Frees the memory allocated for a Package structure.
 *
 * @param package Pointer to the Package structure to free.
 */
void aml_package_deinit(aml_package_t* package);

/**
 * @brief Frees the memory allocated for a DataObject structure.
 *
 * @param obj Pointer to the DataObject structure to free.
 */
static inline void aml_data_object_deinit(aml_data_object_t* obj)
{
    switch (obj->type)
    {
    case AML_DATA_STRING:
        aml_string_deinit(&obj->string);
        break;
    case AML_DATA_BUFFER:
        aml_buffer_deinit(&obj->buffer);
        break;
    case AML_DATA_PACKAGE:
        aml_package_deinit(&obj->package);
        break;
    default:
        // No dynamic memory to free for other types.
        break;
    }
    obj->type = AML_DATA_NONE;
}

/**
 * @brief Puts bits into a DataObject at the specified bit offset and size.
 *
 * Only supports Integer and Buffer types.
 *
 * @param obj Pointer to the DataObject to modify.
 * @param value The value to put into the DataObject.
 * @param bitOffset The bit offset within the DataObject where the value will be placed.
 * @param bitSize The number of bits to write from the value, must be <= 64.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_put_bits_at(aml_data_object_t* obj, uint64_t value, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize);

/**
 * @brief Gets bits from a DataObject at the specified bit offset and size.
 *
 * Only supports Integer, Buffer and String types.
 *
 * @param obj Pointer to the DataObject to read from.
 * @param bitOffset The bit offset within the DataObject where the value will be read from.
 * @param bitSize The number of bits to read from the DataObject, must be <= 64.
 * @param out Pointer to the buffer where the result will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_data_object_get_bits_at(aml_data_object_t* obj, aml_bit_size_t bitOffset, aml_bit_size_t bitSize,
    uint64_t* out);

/** @} */

#pragma once

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"

#include <errno.h>
#include <stdint.h>

/**
 * @ingroup kernel_acpi_aml_data
 *
 * @{
 */

/**
 * @brief ACPI AML ByteData structure.
 */
typedef uint8_t aml_byte_data_t;

/**
 * @brief ACPI AML WordData structure.
 */
typedef uint16_t aml_word_data_t;

/**
 * @brief ACPI AML DWordData structure.
 */
typedef uint32_t aml_dword_data_t;

/**
 * @brief ACPI AML QWordData structure.
 */
typedef uint64_t aml_qword_data_t;

/**
 * @brief ACPI AML ByteConst structure.
 */
typedef aml_byte_data_t aml_byte_const_t;

/**
 * @brief ACPI AML WordConst structure.
 */
typedef aml_word_data_t aml_word_const_t;

/**
 * @brief ACPI AML DWordConst structure.
 */
typedef aml_dword_data_t aml_dword_const_t;

/**
 * @brief ACPI AML QWordConst structure.
 */
typedef aml_qword_data_t aml_qword_const_t;

/**
 * @brief ACPI AML ConstObj structure.
 */
typedef aml_qword_data_t aml_const_obj_t;

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
    AML_DATA_MAX,
} aml_data_type_t;

/**
 * @brief ACPI AML DataObject structure.
 *
 * Represents both the DataObject structure found in the specification, but also used to generalally store any generic
 * data in AML, for example the result of a TermArg evaluation.
 */
typedef struct
{
    aml_data_type_t type;
    union {
        aml_qword_data_t integer;
        struct
        {
            char* content;
            uint64_t length;
        } string;
        struct
        {
            uint8_t* content;
            uint64_t length;
        } buffer;
        struct
        {
            struct aml_data_object* elements;
            uint64_t count;
        } package;
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

/** @} */

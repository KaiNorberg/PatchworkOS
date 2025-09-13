#pragma once

#include "aml_state.h"

#include <errno.h>
#include <stdint.h>

/**
 * @brief ACPI AML Values
 * @defgroup kernel_acpi_aml_value Values
 * @ingroup kernel_acpi_aml
 *
 * This file handles descriptions of all values that can be found in an AML byte stream, storing them and their
 * properties.
 *
 * The value type ends up, in most cases, being the smallest most fundamental type used in the parser (as in the type at
 * the bottom of the recursive tree), but there are some exceptions, for example a ByteData object is not a value type
 * simply because it can have any value.
 *
 * See section 20.3 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief ACPI AML Values
 *
 * All values stored an an enum, we also encode the extended values (those that make up two bytes) by assigning them
 * certain value ranges.
 */
typedef enum
{
    // Normal (0x00-0xFF)
    AML_NULL_NAME = 0x00,
    AML_ZERO_OP = 0x00,
    AML_ONE_OP = 0x01,
    AML_ALIAS_OP = 0x06,
    AML_NAME_OP = 0x08,
    AML_BYTE_PREFIX = 0x0A,
    AML_WORD_PREFIX = 0x0B,
    AML_DWORD_PREFIX = 0x0C,
    AML_STRING_PREFIX = 0x0D,
    AML_QWORD_PREFIX = 0x0E,
    AML_SCOPE_OP = 0x10,
    AML_BUFFER_OP = 0x11,
    AML_PACKAGE_OP = 0x12,
    AML_VAR_PACKAGE_OP = 0x13,
    AML_METHOD_OP = 0x14,
    AML_EXTERNAL_OP = 0x15,
    AML_DUAL_NAME_PREFIX = 0x2E,
    AML_MULTI_NAME_PREFIX = 0x2F,
    AML_DIGIT_CHAR_0 = 0x30,
    AML_DIGIT_CHAR_1 = 0x31,
    AML_DIGIT_CHAR_2 = 0x32,
    AML_DIGIT_CHAR_3 = 0x33,
    AML_DIGIT_CHAR_4 = 0x34,
    AML_DIGIT_CHAR_5 = 0x35,
    AML_DIGIT_CHAR_6 = 0x36,
    AML_DIGIT_CHAR_7 = 0x37,
    AML_DIGIT_CHAR_8 = 0x38,
    AML_DIGIT_CHAR_9 = 0x39,
    AML_NAME_CHAR_A = 0x41,
    AML_NAME_CHAR_B = 0x42,
    AML_NAME_CHAR_C = 0x43,
    AML_NAME_CHAR_D = 0x44,
    AML_NAME_CHAR_E = 0x45,
    AML_NAME_CHAR_F = 0x46,
    AML_NAME_CHAR_G = 0x47,
    AML_NAME_CHAR_H = 0x48,
    AML_NAME_CHAR_I = 0x49,
    AML_NAME_CHAR_J = 0x4A,
    AML_NAME_CHAR_K = 0x4B,
    AML_NAME_CHAR_L = 0x4C,
    AML_NAME_CHAR_M = 0x4D,
    AML_NAME_CHAR_N = 0x4E,
    AML_NAME_CHAR_O = 0x4F,
    AML_NAME_CHAR_P = 0x50,
    AML_NAME_CHAR_Q = 0x51,
    AML_NAME_CHAR_R = 0x52,
    AML_NAME_CHAR_S = 0x53,
    AML_NAME_CHAR_T = 0x54,
    AML_NAME_CHAR_U = 0x55,
    AML_NAME_CHAR_V = 0x56,
    AML_NAME_CHAR_W = 0x57,
    AML_NAME_CHAR_X = 0x58,
    AML_NAME_CHAR_Y = 0x59,
    AML_NAME_CHAR_Z = 0x5A,
    AML_EXT_OP_PREFIX = 0x5B,
    AML_ROOT_CHAR = 0x5C,
    AML_PARENT_PREFIX_CHAR = 0x5E,
    AML_NAME_CHAR = 0x5F,
    AML_LOCAL0_OP = 0x60,
    AML_LOCAL1_OP = 0x61,
    AML_LOCAL2_OP = 0x62,
    AML_LOCAL3_OP = 0x63,
    AML_LOCAL4_OP = 0x64,
    AML_LOCAL5_OP = 0x65,
    AML_LOCAL6_OP = 0x66,
    AML_LOCAL7_OP = 0x67,
    AML_ARG0_OP = 0x68,
    AML_ARG1_OP = 0x69,
    AML_ARG2_OP = 0x6A,
    AML_ARG3_OP = 0x6B,
    AML_ARG4_OP = 0x6C,
    AML_ARG5_OP = 0x6D,
    AML_ARG6_OP = 0x6E,
    AML_STORE_OP = 0x70,
    AML_REF_OF_OP = 0x71,
    AML_ADD_OP = 0x72,
    AML_CONCAT_OP = 0x73,
    AML_SUBTRACT_OP = 0x74,
    AML_INCREMENT_OP = 0x75,
    AML_DECREMENT_OP = 0x76,
    AML_MULTIPLY_OP = 0x77,
    AML_DIVIDE_OP = 0x78,
    AML_SHIFT_LEFT_OP = 0x79,
    AML_SHIFT_RIGHT_OP = 0x7A,
    AML_AND_OP = 0x7B,
    AML_NAND_OP = 0x7C,
    AML_OR_OP = 0x7D,
    AML_NOR_OP = 0x7E,
    AML_XOR_OP = 0x7F,
    AML_NOT_OP = 0x80,
    AML_FIND_SET_LEFT_BIT_OP = 0x81,
    AML_FIND_SET_RIGHT_BIT_OP = 0x82,
    AML_DEREF_OF_OP = 0x83,
    AML_CONCAT_RES_OP = 0x84,
    AML_MOD_OP = 0x85,
    AML_NOTIFY_OP = 0x86,
    AML_SIZE_OF_OP = 0x87,
    AML_INDEX_OP = 0x88,
    AML_MATCH_OP = 0x89,
    AML_CREATE_DWORD_FIELD_OP = 0x8A,
    AML_CREATE_WORD_FIELD_OP = 0x8B,
    AML_CREATE_BYTE_FIELD_OP = 0x8C,
    AML_CREATE_BIT_FIELD_OP = 0x8D,
    AML_OBJECT_TYPE_OP = 0x8E,
    AML_CREATE_QWORD_FIELD_OP = 0x8F,
    AML_LAND_OP = 0x90,
    AML_LOR_OP = 0x91,
    AML_LNOT_OP = 0x92,
    AML_LEQUAL_OP = 0x93,
    AML_LGREATER_OP = 0x94,
    AML_LLESS_OP = 0x95,
    AML_TO_BUFFER_OP = 0x96,
    AML_TO_DECIMAL_STRING_OP = 0x97,
    AML_TO_HEX_STRING_OP = 0x98,
    AML_TO_INTEGER_OP = 0x99,
    AML_TO_STRING_OP = 0x9C,
    AML_COPY_OBJECT_OP = 0x9D,
    AML_MID_OP = 0x9E,
    AML_CONTINUE_OP = 0x9F,
    AML_IF_OP = 0xA0,
    AML_ELSE_OP = 0xA1,
    AML_WHILE_OP = 0xA2,
    AML_NOOP_OP = 0xA3,
    AML_RETURN_OP = 0xA4,
    AML_BREAK_OP = 0xA5,
    AML_BREAK_POINT_OP = 0xCC,
    AML_ONES_OP = 0xFF,

    // Extended values prefixed with 0x5B (0x100-0x1FF range)
    AML_EXT_OP_PREFIX_BASE = 0x100,
    AML_MUTEX_OP = AML_EXT_OP_PREFIX_BASE + 0x01,
    AML_EVENT_OP = AML_EXT_OP_PREFIX_BASE + 0x02,
    AML_COND_REF_OF_OP = AML_EXT_OP_PREFIX_BASE + 0x12,
    AML_CREATE_FIELD_OP = AML_EXT_OP_PREFIX_BASE + 0x13,
    AML_LOAD_TABLE_OP = AML_EXT_OP_PREFIX_BASE + 0x1F,
    AML_LOAD_OP = AML_EXT_OP_PREFIX_BASE + 0x20,
    AML_STALL_OP = AML_EXT_OP_PREFIX_BASE + 0x21,
    AML_SLEEP_OP = AML_EXT_OP_PREFIX_BASE + 0x22,
    AML_ACQUIRE_OP = AML_EXT_OP_PREFIX_BASE + 0x23,
    AML_SIGNAL_OP = AML_EXT_OP_PREFIX_BASE + 0x24,
    AML_WAIT_OP = AML_EXT_OP_PREFIX_BASE + 0x25,
    AML_RESET_OP = AML_EXT_OP_PREFIX_BASE + 0x26,
    AML_RELEASE_OP = AML_EXT_OP_PREFIX_BASE + 0x27,
    AML_FROM_BCD_OP = AML_EXT_OP_PREFIX_BASE + 0x28,
    AML_TO_BCD_OP = AML_EXT_OP_PREFIX_BASE + 0x29,
    AML_RESERVED_OP = AML_EXT_OP_PREFIX_BASE + 0x2A,
    AML_REVISION_OP = AML_EXT_OP_PREFIX_BASE + 0x30,
    AML_DEBUG_OP = AML_EXT_OP_PREFIX_BASE + 0x31,
    AML_FATAL_OP = AML_EXT_OP_PREFIX_BASE + 0x32,
    AML_TIMER_OP = AML_EXT_OP_PREFIX_BASE + 0x33,
    AML_OPREGION_OP = AML_EXT_OP_PREFIX_BASE + 0x80,
    AML_FIELD_OP = AML_EXT_OP_PREFIX_BASE + 0x81,
    AML_DEVICE_OP = AML_EXT_OP_PREFIX_BASE + 0x82,
    AML_POWER_RES_OP = AML_EXT_OP_PREFIX_BASE + 0x84,
    AML_THERMAL_ZONE_OP = AML_EXT_OP_PREFIX_BASE + 0x85,
    AML_INDEX_FIELD_OP = AML_EXT_OP_PREFIX_BASE + 0x86,
    AML_BANK_FIELD_OP = AML_EXT_OP_PREFIX_BASE + 0x87,
    AML_DATA_REGION_OP = AML_EXT_OP_PREFIX_BASE + 0x88,

    // Extended values prefixed with 0x92 (0x200-0x2FF range)
    AML_LNOT_OP_BASE = 0x200,
    AML_LNOT_EQUAL_OP = AML_LNOT_OP_BASE + 0x93,
    AML_LLESS_EQUAL_OP = AML_LNOT_OP_BASE + 0x94,
    AML_LGREATER_EQUAL_OP = AML_LNOT_OP_BASE + 0x95,
} aml_value_num_t;

typedef enum
{
    AML_ENCODING_GROUP_NONE,
    AML_ENCODING_GROUP_DATA,
    AML_ENCODING_GROUP_TERM,
    AML_ENCODING_GROUP_NAME,
    AML_ENCODING_GROUP_LOCAL,
    AML_ENCODING_GROUP_ARG,
    AML_ENCODING_GROUP_DEBUG,
} aml_encoding_group_t;

/**
 * @brief ACPI AML Value Type
 * @enum aml_value_type_t
 */
typedef enum
{
    AML_VALUE_TYPE_NONE = 0,
    AML_VALUE_TYPE_NAME,               //!< Is a Name Object (section 20.2.2).
    AML_VALUE_TYPE_NAMESPACE_MODIFIER, //!< Is a Namespace Modifier Object (section 20.2.5.1).
    AML_VALUE_TYPE_NAMED,              //!< Is a NamedObj (section 20.2.5.2).
    AML_VALUE_TYPE_STATEMENT,          //!< Is a Statement Opcode (section 20.2.5.3).
    AML_VALUE_TYPE_EXPRESSION,         //!< Is an Expression Opcode (section 20.2.5.4).
    AML_VALUE_TYPE_ARG,                //!< Is an Arg Object (section 20.2.6.1).
    AML_VALUE_TYPE_LOCAL,              //!< Is a Local Object (section 20.2.6.2).
    AML_VALUE_TYPE_COMPUTATIONAL,      //!< Is part of a ComputationalData Object (section 20.2.3).
    AML_VALUE_TYPE_DEBUG,              //!< Is a Debug Object (section 20.2.6.3).
} aml_value_type_t;

/**
 * @brief ACPI AML Value Properties
 * @struct aml_value_props_t
 */
typedef struct aml_value_props
{
    const char* name;
    aml_encoding_group_t group;
    aml_value_type_t type;
} aml_value_props_t;

/**
 * @brief ACPI AML Value
 * @struct aml_value_t
 */
typedef struct aml_value
{
    uint64_t index; //!< The index of the first byte of the value in the AML byte stream.
    aml_value_num_t num;
    uint8_t length;
    const aml_value_props_t* props;
} aml_value_t;

/**
 * @brief Normal values without prefix.
 */
extern const aml_value_props_t opsNormal[0x100];

/**
 * @brief Extended values prefixed with 0x5B.
 */
extern const aml_value_props_t opsExt5b[0x100];

/**
 * @brief Extended values prefixed with 0x92.
 */
extern const aml_value_props_t opsExt92[0x100];

/**
 * @brief Convert a value type to a string.
 *
 * @param type The value type to convert.
 * @return const char* On success, the string representation of the value type. On failure, "UnknownValue".
 */
const char* aml_value_type_to_string(aml_value_type_t type);

/**
 * @brief Lookup value properties.
 *
 * @param value The value to fetch properties for.
 * @param extension The prefix byte of the value, use `0` if none.
 * @return const aml_value_props_t* The value properties.
 */
static inline const aml_value_props_t* aml_value_lookup(uint8_t value, uint8_t extension)
{
    const aml_value_props_t* props = NULL;
    if (extension == 0)
    {
        props = &opsNormal[value];
    }
    else if (extension == AML_EXT_OP_PREFIX)
    {
        props = &opsExt5b[value];
    }
    else if (extension == AML_LNOT_OP)
    {
        props = &opsExt92[value];
    }
    else
    {
        return NULL;
    }

    if (props->name == NULL)
    {
        return NULL;
    }

    return props;
}

/**
 * @brief Attempt to read a single byte value from the AML stream, without advancing the instruction pointer.
 *
 * Intended to be used when the value is known to be a single byte for performance reasons.
 *
 * @param state The AML state to parse from.
 * @param out The destination for the parsed value.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set to `ENODATA` if the stream is empty or `EILSEQ`
 * if the current data is not a valid value.
 */
static inline uint64_t aml_value_peek_no_ext(aml_state_t* state, aml_value_t* out)
{
    uint8_t value;
    uint64_t byteAmount = aml_state_peek(state, &value, 1);
    if (byteAmount == ERR || byteAmount == 0)
    {
        errno = ENODATA;
        return ERR;
    }

    const aml_value_props_t* props = aml_value_lookup(value, 0);
    if (props == NULL)
    {
        errno = EILSEQ;
        return ERR;
    }

    out->num = value;
    out->length = 1;
    out->props = props;
    return 0;
}

/**
 * @brief Attempt to read a single byte value from the AML stream.
 *
 * Intended to be used when the value is known to be a single byte for performance reasons.
 *
 * @param state The AML state to parse from.
 * @param out The destination for the parsed value.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set to `ENODATA` if the stream is empty or `EILSEQ`
 * if the current data is not a valid value.
 */
static inline uint64_t aml_value_read_no_ext(aml_state_t* state, aml_value_t* out)
{
    if (aml_value_peek_no_ext(state, out) == ERR)
    {
        return ERR;
    }

    aml_state_advance(state, out->length);
    return 0;
}

/**
 * @brief Attempt to read a value from the AML stream, without advancing the instruction pointer.
 *
 * @param state The AML state to parse from.
 * @param out The destination for the parsed value.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set to `ENODATA` if the stream is empty or `EILSEQ`
 * if the current data is not a valid value.
 */
static inline uint64_t aml_value_peek(aml_state_t* state, aml_value_t* out)
{
    uint8_t buffer[2];
    uint64_t byteAmount = aml_state_peek(state, buffer, sizeof(buffer));
    if (byteAmount == 0 || byteAmount == ERR)
    {
        errno = ENODATA;
        return ERR;
    }

    uint8_t extension = 0;
    uint8_t value = buffer[0];
    uint8_t length = 1;

    if (byteAmount == 2)
    {
        if (buffer[0] == AML_EXT_OP_PREFIX)
        {
            extension = AML_EXT_OP_PREFIX;
            value = buffer[1];
            length = 2;
        }
        else if (buffer[0] == AML_LNOT_OP)
        {
            extension = AML_LNOT_OP;
            value = buffer[1];
            length = 2;
        }
    }

    const aml_value_props_t* props = aml_value_lookup(value, extension);
    if (props == NULL)
    {
        errno = EILSEQ;
        return ERR;
    }

    out->num = (extension == AML_EXT_OP_PREFIX) ? AML_EXT_OP_PREFIX_BASE + value : value;
    out->length = length;
    out->props = props;
    return 0;
}
/**
 * @brief Attempt to read a value from the AML stream.
 *
 * @param state The AML state to parse from.
 * @param out The destination for the parsed value.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set to `ENODATA` if the stream is empty or `EILSEQ`
 * if the current data is not a valid value.
 */
static inline uint64_t aml_value_read(aml_state_t* state, aml_value_t* out)
{
    if (aml_value_peek(state, out) == ERR)
    {
        return ERR;
    }

    aml_state_advance(state, out->length);
    return 0;
}

/** @} */

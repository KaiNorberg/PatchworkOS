#pragma once

#include "aml_state.h"

#include <stdint.h>

/**
 * @brief ACPI AML Ops
 * @defgroup kernel_acpi_aml_op Ops
 * @ingroup kernel_acpi_aml
 *
 * Stores the ACPI AML Ops and their properties. Note the difference between an "Op" and an "Opcode". In the
 * specification, a "Op" is the number specifying the operation to be performed and its followed by the data for that
 * operation. An "Opcode" is the "Op" and that data combined. For example, the "DefScope" opcode is defined as `DefScope
 * := ScopeOp PkgLength NameString Termlist`, where the `ScopeOp` is the "Op" and the `PkgLength NameString Termlist` is
 * the data.
 *
 * @{
 */

/**
 * @brief ACPI AML Ops
 *
 * Stores the all the "Op" values, we also encode the extended opcodes by assigning them certain value ranges.
 */
typedef enum
{
    // Normal opcodes (0x00-0xFF)
    AML_OP_ZERO = 0x00,
    AML_OP_ONE = 0x01,
    AML_OP_ALIAS = 0x06,
    AML_OP_NAME = 0x08,
    AML_OP_BYTE_PREFIX = 0x0A,
    AML_OP_WORD_PREFIX = 0x0B,
    AML_OP_DWORD_PREFIX = 0x0C,
    AML_OP_STRING_PREFIX = 0x0D,
    AML_OP_QWORD_PREFIX = 0x0E,
    AML_OP_SCOPE = 0x10,
    AML_OP_BUFFER = 0x11,
    AML_OP_PACKAGE = 0x12,
    AML_OP_VAR_PACKAGE = 0x13,
    AML_OP_METHOD = 0x14,
    AML_OP_EXTERNAL = 0x15,
    AML_OP_DUAL_NAME_PREFIX = 0x2E,
    AML_OP_MULTI_NAME_PREFIX = 0x2F,
    AML_OP_DIGIT_CHAR_0 = 0x30,
    AML_OP_DIGIT_CHAR_1 = 0x31,
    AML_OP_DIGIT_CHAR_2 = 0x32,
    AML_OP_DIGIT_CHAR_3 = 0x33,
    AML_OP_DIGIT_CHAR_4 = 0x34,
    AML_OP_DIGIT_CHAR_5 = 0x35,
    AML_OP_DIGIT_CHAR_6 = 0x36,
    AML_OP_DIGIT_CHAR_7 = 0x37,
    AML_OP_DIGIT_CHAR_8 = 0x38,
    AML_OP_DIGIT_CHAR_9 = 0x39,
    AML_OP_NAME_CHAR_A = 0x41,
    AML_OP_NAME_CHAR_B = 0x42,
    AML_OP_NAME_CHAR_C = 0x43,
    AML_OP_NAME_CHAR_D = 0x44,
    AML_OP_NAME_CHAR_E = 0x45,
    AML_OP_NAME_CHAR_F = 0x46,
    AML_OP_NAME_CHAR_G = 0x47,
    AML_OP_NAME_CHAR_H = 0x48,
    AML_OP_NAME_CHAR_I = 0x49,
    AML_OP_NAME_CHAR_J = 0x4A,
    AML_OP_NAME_CHAR_K = 0x4B,
    AML_OP_NAME_CHAR_L = 0x4C,
    AML_OP_NAME_CHAR_M = 0x4D,
    AML_OP_NAME_CHAR_N = 0x4E,
    AML_OP_NAME_CHAR_O = 0x4F,
    AML_OP_NAME_CHAR_P = 0x50,
    AML_OP_NAME_CHAR_Q = 0x51,
    AML_OP_NAME_CHAR_R = 0x52,
    AML_OP_NAME_CHAR_S = 0x53,
    AML_OP_NAME_CHAR_T = 0x54,
    AML_OP_NAME_CHAR_U = 0x55,
    AML_OP_NAME_CHAR_V = 0x56,
    AML_OP_NAME_CHAR_W = 0x57,
    AML_OP_NAME_CHAR_X = 0x58,
    AML_OP_NAME_CHAR_Y = 0x59,
    AML_OP_NAME_CHAR_Z = 0x5A,
    AML_OP_EXT_PREFIX = 0x5B,
    AML_OP_ROOT_CHAR = 0x5C,
    AML_OP_PARENT_PREFIX_CHAR = 0x5E,
    AML_OP_NAME_CHAR_UNDERSCORE = 0x5F,
    AML_OP_LOCAL0 = 0x60,
    AML_OP_LOCAL1 = 0x61,
    AML_OP_LOCAL2 = 0x62,
    AML_OP_LOCAL3 = 0x63,
    AML_OP_LOCAL4 = 0x64,
    AML_OP_LOCAL5 = 0x65,
    AML_OP_LOCAL6 = 0x66,
    AML_OP_LOCAL7 = 0x67,
    AML_OP_ARG0 = 0x68,
    AML_OP_ARG1 = 0x69,
    AML_OP_ARG2 = 0x6A,
    AML_OP_ARG3 = 0x6B,
    AML_OP_ARG4 = 0x6C,
    AML_OP_ARG5 = 0x6D,
    AML_OP_ARG6 = 0x6E,
    AML_OP_STORE = 0x70,
    AML_OP_REF_OF = 0x71,
    AML_OP_ADD = 0x72,
    AML_OP_CONCAT = 0x73,
    AML_OP_SUBTRACT = 0x74,
    AML_OP_INCREMENT = 0x75,
    AML_OP_DECREMENT = 0x76,
    AML_OP_MULTIPLY = 0x77,
    AML_OP_DIVIDE = 0x78,
    AML_OP_SHIFT_LEFT = 0x79,
    AML_OP_SHIFT_RIGHT = 0x7A,
    AML_OP_AND = 0x7B,
    AML_OP_NAND = 0x7C,
    AML_OP_OR = 0x7D,
    AML_OP_NOR = 0x7E,
    AML_OP_XOR = 0x7F,
    AML_OP_NOT = 0x80,
    AML_OP_FIND_SET_LEFT_BIT = 0x81,
    AML_OP_FIND_SET_RIGHT_BIT = 0x82,
    AML_OP_DEREF_OF = 0x83,
    AML_OP_CONCAT_RES = 0x84,
    AML_OP_MOD = 0x85,
    AML_OP_NOTIFY = 0x86,
    AML_OP_SIZE_OF = 0x87,
    AML_OP_INDEX = 0x88,
    AML_OP_MATCH = 0x89,
    AML_OP_CREATE_DWORD_FIELD = 0x8A,
    AML_OP_CREATE_WORD_FIELD = 0x8B,
    AML_OP_CREATE_BYTE_FIELD = 0x8C,
    AML_OP_CREATE_BIT_FIELD = 0x8D,
    AML_OP_OBJECT_TYPE = 0x8E,
    AML_OP_CREATE_QWORD_FIELD = 0x8F,
    AML_OP_LAND = 0x90,
    AML_OP_LOR = 0x91,
    AML_OP_LNOT = 0x92,
    AML_OP_LEQUAL = 0x93,
    AML_OP_LGREATER = 0x94,
    AML_OP_LLESS = 0x95,
    AML_OP_TO_BUFFER = 0x96,
    AML_OP_TO_DECIMAL_STRING = 0x97,
    AML_OP_TO_HEX_STRING = 0x98,
    AML_OP_TO_INTEGER = 0x99,
    AML_OP_TO_STRING = 0x9C,
    AML_OP_COPY_OBJECT = 0x9D,
    AML_OP_MID = 0x9E,
    AML_OP_CONTINUE = 0x9F,
    AML_OP_IF = 0xA0,
    AML_OP_ELSE = 0xA1,
    AML_OP_WHILE = 0xA2,
    AML_OP_NOOP = 0xA3,
    AML_OP_RETURN = 0xA4,
    AML_OP_BREAK = 0xA5,
    AML_OP_BREAK_POINT = 0xCC,
    AML_OP_ONES = 0xFF,

    // Extended opcodes prefixed with 0x5B (0x100-0x1FF range)
    AML_OP_EXT5B_BASE = 0x100,
    AML_OP_MUTEX = AML_OP_EXT5B_BASE + 0x01,
    AML_OP_EVENT = AML_OP_EXT5B_BASE + 0x02,
    AML_OP_COND_REF_OF = AML_OP_EXT5B_BASE + 0x12,
    AML_OP_CREATE_FIELD = AML_OP_EXT5B_BASE + 0x13,
    AML_OP_LOAD_TABLE = AML_OP_EXT5B_BASE + 0x1F,
    AML_OP_LOAD = AML_OP_EXT5B_BASE + 0x20,
    AML_OP_STALL = AML_OP_EXT5B_BASE + 0x21,
    AML_OP_SLEEP = AML_OP_EXT5B_BASE + 0x22,
    AML_OP_ACQUIRE = AML_OP_EXT5B_BASE + 0x23,
    AML_OP_SIGNAL = AML_OP_EXT5B_BASE + 0x24,
    AML_OP_WAIT = AML_OP_EXT5B_BASE + 0x25,
    AML_OP_RESET = AML_OP_EXT5B_BASE + 0x26,
    AML_OP_RELEASE = AML_OP_EXT5B_BASE + 0x27,
    AML_OP_FROM_BCD = AML_OP_EXT5B_BASE + 0x28,
    AML_OP_TO_BCD = AML_OP_EXT5B_BASE + 0x29,
    AML_OP_RESERVED = AML_OP_EXT5B_BASE + 0x2A,
    AML_OP_REVISION = AML_OP_EXT5B_BASE + 0x30,
    AML_OP_DEBUG = AML_OP_EXT5B_BASE + 0x31,
    AML_OP_FATAL = AML_OP_EXT5B_BASE + 0x32,
    AML_OP_TIMER = AML_OP_EXT5B_BASE + 0x33,
    AML_OP_OPREGION = AML_OP_EXT5B_BASE + 0x80,
    AML_OP_FIELD = AML_OP_EXT5B_BASE + 0x81,
    AML_OP_DEVICE = AML_OP_EXT5B_BASE + 0x82,
    AML_OP_POWER_RES = AML_OP_EXT5B_BASE + 0x84,
    AML_OP_THERMAL_ZONE = AML_OP_EXT5B_BASE + 0x85,
    AML_OP_INDEX_FIELD = AML_OP_EXT5B_BASE + 0x86,
    AML_OP_BANK_FIELD = AML_OP_EXT5B_BASE + 0x87,
    AML_OP_DATA_REGION = AML_OP_EXT5B_BASE + 0x88,

    // Extended opcodes prefixed with 0x92 (0x200-0x2FF range)
    AML_OP_EXT92_BASE = 0x200,
    AML_OP_LNOT_EQUAL = AML_OP_EXT92_BASE + 0x93,
    AML_OP_LLESS_EQUAL = AML_OP_EXT92_BASE + 0x94,
    AML_OP_LGREATER_EQUAL = AML_OP_EXT92_BASE + 0x95,
} aml_op_num_t;

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
 * @brief ACPI AML Op Flags
 * @enum aml_op_flags_t
 */
typedef enum
{
    AML_OP_FLAG_NONE = 0,
    AML_OP_FLAG_NAMESPACE_MODIFIER = 1 << 0, //!< Is a NameSpaceModifierObj. TODO: Fill in the table
    AML_OP_FLAG_NAMED = 1 << 1,              //!< Is a NamedObj. TODO: Fill in the table
    AML_OP_FLAG_STATEMENT = 1 << 2,          //!< Is a StatementOp.
    AML_OP_FLAG_EXPRESSION = 1 << 3,         //!< Is an ExpressionOp.
    AML_OP_FLAG_ANY = -1,
} aml_op_flags_t;

/**
 * @brief ACPI AML Op Properties
 * @struct aml_op_props_t
 */
typedef struct aml_op_props
{
    const char* name;
    aml_encoding_group_t group;
    aml_op_flags_t flags;
} aml_op_props_t;

/**
 * @brief ACPI AML Op
 * @struct aml_op_t
 */
typedef struct aml_op
{
    aml_op_num_t num;
    const aml_op_props_t* props;
} aml_op_t;

/**
 * @brief Lookup op properties.
 *
 * @param op The op to fetch properties for.
 * @param extension The prefix byte of the op, use `0` if none.
 * @return const aml_op_props_t* The op properties.
 */
const aml_op_props_t* aml_op_lookup(uint8_t op, uint8_t extension);

/**
 * @brief Attempt to read an op from the AML stream.
 *
 * @param state The AML state to parse from.
 * @param out The destination for the parsed op.
 * @param flags The flags that the op must have for it to be valid.
 * @return uint64_t If the present data is a valid op, 0. Otherwise, `ERR`.
 */
uint64_t aml_op_read(aml_state_t* state, aml_op_t* out, aml_op_flags_t flags);

/** @} */

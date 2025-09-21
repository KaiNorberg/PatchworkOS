#pragma once

#include "arg.h"
#include "data.h"

typedef struct aml_node aml_node_t;
typedef struct aml_state aml_state_t;

/**
 * @brief ACPI AML Expression Opcodes Encoding
 * @defgroup kernel_acpi_aml_expression Expression Opcodes
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.5.4 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief ACPI AML BufferSize structure.
 */
typedef aml_qword_data_t aml_buffer_size_t;

/**
 * @brief Reads a BufferSize structure from the AML byte stream.
 *
 * A BufferSize structure is defined as `BufferSize := TermArg => Integer`.
 *
 * @see Section 19.6.10 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the buffer size will be stored.
 * @return On success, the buffer size. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_size_read(aml_state_t* state, aml_buffer_size_t* out);

/**
 * @brief Reads a DefBuffer structure from the AML byte stream.
 *
 * The DefBuffer structure is defined as `DefBuffer := BufferOp PkgLength BufferSize ByteList`.
 *
 * @see Section 19.6.10 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param out Pointer to the buffer where the Buffer will be stored. This will point to a location within the AML
 * bytestream and should not be freed or modified.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_buffer_read(aml_state_t* state, aml_buffer_t* out);

/**
 * @brief ACPI AML TermArgList structure.
 */
typedef struct
{
    aml_data_object_t args[AML_MAX_ARGS];
    uint8_t count;
} aml_term_arg_list_t;

/**
 * @brief Reads a TermArgList structure from the AML byte stream.
 *
 * A TermArgList structure is defined as `TermArgList := Nothing | <termarg termarglist>`.
 *
 * The number of arguments to read is determined by knowing ahead of time what node the arguments will be passed to.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param argCount The number of arguments to read.
 * @param out Pointer to the buffer where the TermArgList will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_list_read(aml_state_t* state, aml_node_t* node, uint64_t argCount, aml_term_arg_list_t* out);

/**
 * @brief Reads a MethodInvocation structure from the AML byte stream.
 *
 * A MethodInvocation structure is defined as `MethodInvocation := NameString TermArgList`.
 *
 * Despite the name, a MethodInvocation can be used to evaluate any node, not just methods. For example, fields.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the method invocation will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_invocation_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads a DefCondRefOf structure from the AML byte stream.
 *
 * A DefCondRefOf structure is defined as `DefCondRefOf := CondRefOfOp SuperName Target`.
 *
 * @see Section 19.6.14 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the CondRefOf will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_cond_ref_of_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads a DefStore structure from the AML byte stream.
 *
 * A DefStore structure is defined as `DefStore := StoreOp TermArg SuperName`.
 *
 * @see Section 19.6.132 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the value moved by the Store operation will also be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_store_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads an Operand structure from the AML byte stream.
 *
 * An Operand structure is defined as `Operand := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the operand will be stored, will always be an integer.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_operand_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads a Dividend structure from the AML byte stream.
 *
 * A Dividend structure is defined as `Dividend := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the dividend will be stored, will always be an integer.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dividend_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads a Divisor structure from the AML byte stream.
 *
 * A Divisor structure is defined as `Divisor := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the divisor will be stored, will always be an integer.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_divisor_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads a Remainder structure from the AML byte stream.
 *
 * A Remainder structure is defined as `Remainder := Target`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the object reference to store the remainder will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_remainder_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out);

/**
 * @brief Reads a Quotient structure from the AML byte stream.
 *
 * A Quotient structure is defined as `Quotient := Target`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the object reference to store the quotient will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_quotient_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out);

/**
 * @brief Reads a DefAdd structure from the AML byte stream.
 *
 * The DefAdd structure is defined as `DefAdd := AddOp Operand Operand Target`.
 *
 * @see Section 19.6.3 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the addition will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_add_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads a DefSubtract structure from the AML byte stream.
 *
 * The DefSubtract structure is defined as `DefSubtract := SubtractOp Operand Operand Target`.
 *
 * @see Section 19.6.133 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the subtraction will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_subtract_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads a DefMultiply structure from the AML byte stream.
 *
 * The DefMultiply structure is defined as `DefMultiply := MultiplyOp Operand Operand Target`.
 *
 * @see Section 19.6.88 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the multiplication will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_multiply_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads a DefDivide structure from the AML byte stream.
 *
 * The DefDivide structure is defined as `DefDivide := DivideOp Dividend Divisor Remainder Quotient`.
 *
 * @see Section 19.6.32 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the division will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_divide_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/**
 * @brief Reads an ExpressionOpcode structure from the AML byte stream.
 *
 * An ExpressionOpcode structure is defined as `ExpressionOpcode := DefAcquire | DefAdd | DefAnd | DefBuffer | DefConcat
 * | DefConcatRes | DefCondRefOf | DefCopyObject | DefDecrement | DefDerefOf | DefDivide | DefFindSetLeftBit |
 * DefFindSetRightBit | DefFromBCD | DefIncrement | DefIndex | DefLAnd | DefLEqual | DefLGreater | DefLGreaterEqual |
 * DefLLess | DefLLessEqual | DefMid | DefLNot | DefLNotEqual | DefLoadTable | DefLOr | DefMatch | DefMod | DefMultiply
 * | DefNAnd | DefNOr | DefNot | DefObjectType | DefOr | DefPackage | DefVarPackage | DefRefOf | DefShiftLeft |
 * DefShiftRight | DefSizeOf | DefStore | DefSubtract | DefTimer | DefToBCD | DefToBuffer | DefToDecimalString |
 * DefToHexString | DefToInteger | DefToString | DefWait | DefXOr | MethodInvocation`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the expression will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_expression_opcode_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/** @} */

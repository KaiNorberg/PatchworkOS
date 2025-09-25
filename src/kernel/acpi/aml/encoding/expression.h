#pragma once

#include "acpi/aml/aml_node.h"
#include "arg.h"
#include "data.h"

typedef struct aml_state aml_state_t;

/**
 * @brief Expression Opcodes Encoding
 * @defgroup kernel_acpi_aml_expression Expression Opcodes
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.5.4 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief TermArgList structure.
 */
typedef struct
{
    aml_node_t args[AML_MAX_ARGS];
    uint8_t count;
} aml_term_arg_list_t;

/**
 * @brief Reads a BufferSize structure from the AML byte stream.
 *
 * A BufferSize structure is defined as `BufferSize := TermArg => Integer`.
 *
 * @see Section 19.6.10 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the buffer size will be stored.
 * @return On success, the buffer size. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_size_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out);

/**
 * @brief Reads a DefBuffer structure from the AML byte stream.
 *
 * The DefBuffer structure is defined as `DefBuffer := BufferOp PkgLength BufferSize ByteList`.
 *
 * @see Section 19.6.10 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the buffer will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_buffer_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

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
 * So this is a bit confusing, but my interpretation is that despite the name, a MethodInvocation can be any node, not
 * just methods. For example, fields. In such cases, the TermArgList is empty. Its the only thing that makes any sense
 * when I inspect the aml bytecode as there are clearly named objects referenced in TermArgs, but there is no "child"
 * definition that contains such a thing, atleast that i can find. But the specification says literally nothing about
 * this.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node which will be initalized as a ObjectReference to the resolved node or the result of
 * the method.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_invocation_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefCondRefOf structure from the AML byte stream.
 *
 * A DefCondRefOf structure is defined as `DefCondRefOf := CondRefOfOp SuperName Target`.
 *
 * @see Section 19.6.14 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the CondRefOf will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_cond_ref_of_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefStore structure from the AML byte stream.
 *
 * A DefStore structure is defined as `DefStore := StoreOp TermArg SuperName`.
 *
 * @see Section 19.6.132 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the value moved by the Store operation will also be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_store_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads an Operand structure from the AML byte stream.
 *
 * An Operand structure is defined as `Operand := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the operand will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_operand_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out);

/**
 * @brief Reads a Dividend structure from the AML byte stream.
 *
 * A Dividend structure is defined as `Dividend := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the dividend will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dividend_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out);

/**
 * @brief Reads a Divisor structure from the AML byte stream.
 *
 * A Divisor structure is defined as `Divisor := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the divisor will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_divisor_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out);

/**
 * @brief Reads a Remainder structure from the AML byte stream.
 *
 * A Remainder structure is defined as `Remainder := Target`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to where the pointer to the resolved node will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_remainder_read(aml_state_t* state, aml_node_t* node, aml_node_t** out);

/**
 * @brief Reads a Quotient structure from the AML byte stream.
 *
 * A Quotient structure is defined as `Quotient := Target`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to where the pointer to the resolved node will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_quotient_read(aml_state_t* state, aml_node_t* node, aml_node_t** out);

/**
 * @brief Reads a DefAdd structure from the AML byte stream.
 *
 * The DefAdd structure is defined as `DefAdd := AddOp Operand Operand Target`.
 *
 * @see Section 19.6.3 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the addition will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_add_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefSubtract structure from the AML byte stream.
 *
 * The DefSubtract structure is defined as `DefSubtract := SubtractOp Operand Operand Target`.
 *
 * @see Section 19.6.133 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the subtraction will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_subtract_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefMultiply structure from the AML byte stream.
 *
 * The DefMultiply structure is defined as `DefMultiply := MultiplyOp Operand Operand Target`.
 *
 * @see Section 19.6.88 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the multiplication will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_multiply_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefDivide structure from the AML byte stream.
 *
 * The DefDivide structure is defined as `DefDivide := DivideOp Dividend Divisor Remainder Quotient`.
 *
 * @see Section 19.6.32 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the division will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_divide_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefMod structure from the AML byte stream.
 *
 * The DefMod structure is defined as `DefMod := ModOp Dividend Divisor Target`.
 *
 * @see Section 19.6.87 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the modulus operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_mod_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefAnd structure from the AML byte stream.
 *
 * The DefAnd structure is defined as `DefAnd := AndOp Operand Operand Target`.
 *
 * @see Section 19.6.5 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the AND operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_and_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefNAnd structure from the AML byte stream.
 *
 * The DefNAnd structure is defined as `DefNAnd := NandOp Operand Operand Target`.
 *
 * @see Section 19.6.69 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the NAND operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_nand_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefOr structure from the AML byte stream.
 *
 * The DefOr structure is defined as `DefOr := OrOp Operand Operand Target`.
 *
 * @see Section 19.6.100 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the OR operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_or_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefNOr structure from the AML byte stream.
 *
 * The DefNOr structure is defined as `DefNOr := NorOp Operand Operand Target`.
 *
 * @see Section 19.6.93 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the NOR operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_nor_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefXOr structure from the AML byte stream.
 *
 * The DefXOr structure is defined as `DefXOr := XorOp Operand Operand Target`.
 *
 * @see Section 19.6.155 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the XOR operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_xor_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefNot structure from the AML byte stream.
 *
 * The DefNot structure is defined as `DefNot := NotOp Operand Target`.
 *
 * @see Section 19.6.94 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the NOT operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_not_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a ShiftCount structure from the AML byte stream.
 *
 * A ShiftCount structure is defined as `ShiftCount := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the shift count will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_shift_count_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out);

/**
 * @brief Reads a DefShiftLeft structure from the AML byte stream.
 *
 * The DefShiftLeft structure is defined as `DefShiftLeft := ShiftLeftOp Operand ShiftCount Target`.
 *
 * @see Section 19.6.123 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the shift left operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_shift_left_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefShiftRight structure from the AML byte stream.
 *
 * The DefShiftRight structure is defined as `DefShiftRight := ShiftRightOp Operand ShiftCount Target`.
 *
 * @see Section 19.6.124 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the shift right operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_shift_right_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefIncrement structure from the AML byte stream.
 *
 * The DefIncrement structure is defined as `DefIncrement := IncrementOp SuperName`.
 *
 * @see Section 19.6.62 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the increment operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_increment_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefDecrement structure from the AML byte stream.
 *
 * The DefDecrement structure is defined as `DefDecrement := DecrementOp SuperName`.
 *
 * @see Section 19.6.27 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the result of the decrement operation will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_decrement_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads an ObjReference structure from the AML byte stream.
 *
 * An ObjReference structure is defined as `ObjReference := TermArg => ObjectReference | String`.
 *
 * If a String is read then it is considered a path to an object and will be resolved to an ObjectReference.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to where the pointer to the resolved node will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_obj_reference_read(aml_state_t* state, aml_node_t* node, aml_node_t** out);

/**
 * @brief Reads a DefDerefOf structure from the AML byte stream.
 *
 * A DefDerefOf structure is defined as `DefDerefOf := DerefOfOp ObjReference`.
 *
 * @see Section 19.6.30 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the data that was dereferenced will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_deref_of_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a BuffPkgStrObj structure from the AML byte stream.
 *
 * A BuffPkgStrObj structure is defined as `BuffPkgStrObj := TermArg => Buffer, Package, or String`.
 *
 * Note that the TermArg must resolve to an ObjectReference that points to a Buffer, Package, or String.
 * Becouse taking a reference to an node within a temporary object does not make sense, temporary objects are not
 * allowed.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to where the pointer to the resolved Buffer, Package, or String node will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buff_pkg_str_obj_read(aml_state_t* state, aml_node_t* node, aml_node_t** out);

/**
 * @brief Reads an IndexValue structure from the AML byte stream.
 *
 * An IndexValue structure is defined as `IndexValue := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the index value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_index_value_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out);

/**
 * @brief Reads a DefIndex structure from the AML byte stream.
 *
 * A DefIndex structure is defined as `DefIndex := IndexOp BuffPkgStrObj IndexValue Target`.
 *
 * Returns a reference to an indexed element within the buffer, package or string stored in BuffPkgStrObj, and
 * optionally stores that reference in Target.
 *
 * @see Section 19.6.63 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the node where the reference to the indexed element will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_index_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

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
 * @param out Pointer to the node where the result of the expression will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_expression_opcode_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/** @} */

#pragma once

#include "acpi/aml/aml_state.h"
#include "arg.h"
#include "data.h"

typedef struct aml_node aml_node_t;

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
 * @return uint64_t On success, the buffer size. On failure, `ERR` and `errno` is set.
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
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
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
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_list_read(aml_state_t* state, aml_node_t* node, uint8_t argCount, aml_term_arg_list_t* out);

/**
 * @brief Reads a MethodInvocation structure from the AML byte stream.
 *
 * A MethodInvocation structure is defined as `MethodInvocation := NameString TermArgList`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out Pointer to the buffer where the result of the method invocation will be stored.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_invocation_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

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
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_expression_opcode_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out);

/** @} */

#pragma once

#include "data.h"

#include <stdint.h>

typedef struct aml_state aml_state_t;

/**
 * @brief Statement Opcodes Encoding
 * @defgroup kernel_acpi_aml_statement Statement Opcodes
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.5.3 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Reads a Predicate structure from the AML byte stream.
 *
 * A Predicate structure is defined as `Predicate := TermArg => Integer`.
 *
 * @param state The AML state to parse from.
 * @param node The current AML node.
 * @param out The destination buffer to store the integer value of the Predicate.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_predicate_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out);

/**
 * @brief Reads a DefElse structure from the AML byte stream.
 *
 * A DefElse structure is defined as `DefElse := Nothing | <elseop pkglength termlist>`.
 *
 * The Else part of an IfElse statement is optional, so this function takes a `shouldExecute` parameter to indicate
 * whether the TermList should be executed or skipped.
 *
 * @see Section 19.6.39 of the ACPI specification for more details.
 *
 * @param state The AML state to parse from.
 * @param node The current AML node.
 * @param shouldExecute Whether the TermList should be executed or skipped.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_else_read(aml_state_t* state, aml_node_t* node, bool shouldExecute);

/**
 * @brief Reads an DefIfElse structure from the AML byte stream.
 *
 * A DefIfElse structure is defined as `DefIfElse := IfOp PkgLength Predicate TermList DefElse`.
 *
 * The If statment works by evaluating the Predicate, if it is a non-zero integer, the TermList following the Predicate
 * is executed, otherwise if there is a DefElse part, it is executed instead.
 *
 * Note that the the DefIfElse structure is also used for normal If statements, without a "Else" part, this is because
 * the DefElse part is optional.
 *
 * @see Section 19.6.60 of the ACPI specification for more details.
 *
 * @param state The AML state to parse from.
 * @param node The current AML node.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_if_else_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a DefNoop structure from the AML byte stream.
 *
 * A DefNoop structure is defined as `DefNoop := NoopOp`.
 *
 * A Noop does nothing.
 *
 * @param state The AML state to parse from.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_noop_read(aml_state_t* state);

/**
 * @brief Reads an ArgObject structure from the AML byte stream.
 *
 * An ArgObject structure is defined as `ArgObject := TermArg => DataRefObject`.
 *
 * @see Section 19.6.119 of the ACPI specification for more details.
 *
 * @param state The AML state to parse from.
 * @param node The current AML node.
 * @param out Pointer to the node to store the result.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_arg_object_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Reads a DefReturn structure from the AML byte stream.
 *
 * A DefReturn structure is defined as `DefReturn := ReturnOp ArgObject`.
 *
 * @see Section 19.6.120 of the ACPI specification for more details.
 *
 * @param state The AML state to parse from.
 * @param node The current AML node.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_return_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads an StatementOpcode structure from the AML byte stream.
 *
 * A StatementOpcode structure is defined as `StatementOpcode := DefBreak | DefBreakPoint | DefContinue | DefFatal |
 * DefIfElse | DefNoop | DefNotify | DefRelease | DefReset | DefReturn | DefSignal | DefSleep | DefStall | DefWhile`.
 *
 * @param state The AML state to parse from.
 * @param node The current AML node.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_statement_opcode_read(aml_state_t* state, aml_node_t* node);

/** @} */

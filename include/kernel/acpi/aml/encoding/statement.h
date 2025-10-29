#pragma once

#include <kernel/acpi/aml/integer.h>

#include <stdint.h>

typedef struct aml_state aml_state_t;
typedef struct aml_object aml_object_t;
typedef struct aml_term_list_ctx aml_term_list_ctx_t;

/**
 * @brief Statement Opcodes Encoding
 * @defgroup kernel_acpi_aml_encoding_statement Statement Opcodes
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
 * @param ctx The context of the TermList that this structure is part of.
 * @param out The destination buffer to store the integer value of the Predicate.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_predicate_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Reads a DefElse structure from the AML byte stream.
 *
 * A DefElse structure is defined as `DefElse := Nothing | <elseop pkglength termlist>`.
 *
 * @see Section 19.6.39 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param shouldExecute Whether the TermList should be executed or skipped.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_else_read(aml_term_list_ctx_t* ctx, bool shouldExecute);

/**
 * @brief Reads an DefIfElse structure from the AML byte stream.
 *
 * A DefIfElse structure is defined as `DefIfElse := IfOp PkgLength Predicate TermList DefElse`.
 *
 * Note that the the DefIfElse structure is also used for normal If statements, just without a "Else" part, this is
 * because the DefElse part is optional.
 *
 * @see Section 19.6.60 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_if_else_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefNoop structure from the AML byte stream.
 *
 * A DefNoop structure is defined as `DefNoop := NoopOp`.
 *
 * A Noop does nothing.
 *
 * @param state Pointer to the current AML state.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_noop_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads an ArgObject structure from the AML byte stream.
 *
 * An ArgObject structure is defined as `ArgObject := TermArg => DataRefObject`.
 *
 * @see Section 19.6.119 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, the ArgObject. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_arg_object_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefReturn structure from the AML byte stream.
 *
 * A DefReturn structure is defined as `DefReturn := ReturnOp ArgObject`.
 *
 * @see Section 19.6.120 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_return_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefBreak structure from the AML byte stream.
 *
 * A DefBreak structure is defined as `DefBreak := BreakOp`.
 *
 * @see Section 19.6.8 of the ACPI specification for more details.
 *
 * @param state Pointer to the current AML state.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_break_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefContinue structure from the AML byte stream.
 *
 * A DefContinue structure is defined as `DefContinue := ContinueOp`.
 *
 * @see Section 19.6.16 of the ACPI specification for more details.
 *
 * @param state Pointer to the current AML state.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_continue_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefRelease structure from the AML byte stream.
 *
 * A DefRelease structure is defined as `DefRelease := ReleaseOp MutexObject`.
 *
 * @see Section 19.6.117 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_release_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefWhile structure from the AML byte stream.
 *
 * A DefWhile structure is defined as `DefWhile := WhileOp PkgLength Predicate TermList`.
 *
 * The While statement works by evaluating the Predicate, if it is a non-zero integer, the TermList following the
 * Predicate is executed, then the Predicate is evaluated again, this continues until the Predicate evaluates to zero.
 *
 * @see Section 19.6.158 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_while_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads an StatementOpcode structure from the AML byte stream.
 *
 * A StatementOpcode structure is defined as `StatementOpcode := DefBreak | DefBreakPoint | DefContinue | DefFatal |
 * DefIfElse | DefNoop | DefNotify | DefRelease | DefReset | DefReturn | DefSignal | DefSleep | DefStall | DefWhile`.
 *
 * Currently unimplemented Opcodes are:
 * - `DefBreakPoint`
 * - `DefFatal`
 * - `DefNotify`
 * - `DefReset`
 * - `DefSignal`
 * - `DefSleep`
 * - `DefStall`
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_statement_opcode_read(aml_term_list_ctx_t* ctx);

/** @} */

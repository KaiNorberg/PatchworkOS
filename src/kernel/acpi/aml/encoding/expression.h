#pragma once

#include "acpi/aml/object.h"
#include "arg.h"
#include "data.h"

typedef struct aml_object aml_object_t;
typedef struct aml_term_list_ctx aml_term_list_ctx_t;

/**
 * @brief Expression Opcodes Encoding
 * @defgroup kernel_acpi_aml_encoding_expression Expression Opcodes
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.5.4 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief TermArgList structure.
 * @struct aml_term_arg_list_t
 */
typedef struct aml_term_arg_list
{
    aml_object_t* args[AML_MAX_ARGS + 1]; // null-terminated
} aml_term_arg_list_t;

/**
 * @brief Reads an Operand structure from the AML byte stream.
 *
 * An Operand structure is defined as `Operand := TermArg => Integer` in the spec but this must be wrong.
 *
 * For example, Operand is used in the definition of DefLGreaterEqual which is defined as `DefLGreaterEqual :=
 * LgreaterEqualOp Operand Operand`. Clearly using the Operand structure.
 *
 * However, this does not make any sense as in section 19.6.72, regarding LGreaterEqual, it states that "Source1 and
 * Source2 must each evaluate to an integer, a string or a buffer" clearly contradicting the definition of Operand as
 * only being able to evaluate to an integer. More examples of this can be found all over the place.
 *
 * So instead we use let the caller specify what types are allowed.
 *
 * @param ctx The TermList context.
 * @param out Output pointer to be filled with the object pointer storing the result.
 * @param allowedTypes The allowed types that the TermArg can evaluate to.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
aml_object_t* aml_operand_read(aml_term_list_ctx_t* ctx, aml_type_t allowedTypes);

/**
 * @brief Reads a BufferSize structure from the AML byte stream.
 *
 * A BufferSize structure is defined as `BufferSize := TermArg => Integer`.
 *
 * @see Section 19.6.10 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the buffer size will be stored.
 * @return On success, the buffer size. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_buffer_size_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Reads a DefBuffer structure from the AML byte stream.
 *
 * The DefBuffer structure is defined as `DefBuffer := BufferOp PkgLength BufferSize ByteList`.
 *
 * @see Section 19.6.10 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @param out Output pointer to the object to store the result.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_buffer_read(aml_term_list_ctx_t* ctx, aml_object_t* out);

/**
 * @brief Reads a TermArgList structure from the AML byte stream.
 *
 * A TermArgList structure is defined as `TermArgList := Nothing | <termarg termarglist>`.
 *
 * The number of arguments to read is determined by knowing ahead of time what object the arguments will be passed to.
 *
 * @param ctx The TermList context.
 * @param argCount The number of arguments to read.
 * @param out Pointer to the buffer where the TermArgList will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_list_read(aml_term_list_ctx_t* ctx, uint64_t argCount, aml_term_arg_list_t* out);

/**
 * @brief Reads a MethodInvocation structure from the AML byte stream.
 *
 * A MethodInvocation structure is defined as `MethodInvocation := NameString TermArgList`.
 *
 * So this is a bit confusing, but my interpretation is that despite the name, a MethodInvocation can be any object, not
 * just methods. For example, fields. In such cases, the TermArgList is empty. Its the only thing that makes any sense
 * when I inspect the aml bytecode as there are clearly named objects referenced in TermArgs, but there is no "child"
 * definition that contains such a thing, atleast that i can find. But the specification says literally nothing about
 * this. I guess you could say that any object is being "invoked" when it is being read, it just happens to not take any
 * arguments.
 *
 * The result may have the `AML_OBJECT_EXCEPTION_ON_USE` flag set.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_method_invocation_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefCondRefOf structure from the AML byte stream.
 *
 * A DefCondRefOf structure is defined as `DefCondRefOf := CondRefOfOp SuperName Target`.
 *
 * @see Section 19.6.14 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_cond_ref_of_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefStore structure from the AML byte stream.
 *
 * A DefStore structure is defined as `DefStore := StoreOp TermArg SuperName`.
 *
 * @see Section 19.6.132 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_store_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a Dividend structure from the AML byte stream.
 *
 * A Dividend structure is defined as `Dividend := TermArg => Integer`.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the integer value of the dividend will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_dividend_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Reads a Divisor structure from the AML byte stream.
 *
 * A Divisor structure is defined as `Divisor := TermArg => Integer`.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the integer value of the divisor will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_divisor_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Reads a Remainder structure from the AML byte stream.
 *
 * A Remainder structure is defined as `Remainder := Target`.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_remainder_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a Quotient structure from the AML byte stream.
 *
 * A Quotient structure is defined as `Quotient := Target`.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_quotient_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefAdd structure from the AML byte stream.
 *
 * The DefAdd structure is defined as `DefAdd := AddOp Operand Operand Target`.
 *
 * @see Section 19.6.3 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_add_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefSubtract structure from the AML byte stream.
 *
 * The DefSubtract structure is defined as `DefSubtract := SubtractOp Operand Operand Target`.
 *
 * @see Section 19.6.133 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_subtract_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefMultiply structure from the AML byte stream.
 *
 * The DefMultiply structure is defined as `DefMultiply := MultiplyOp Operand Operand Target`.
 *
 * @see Section 19.6.88 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_multiply_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefDivide structure from the AML byte stream.
 *
 * The DefDivide structure is defined as `DefDivide := DivideOp Dividend Divisor Remainder Quotient`.
 *
 * @see Section 19.6.32 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_divide_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefMod structure from the AML byte stream.
 *
 * The DefMod structure is defined as `DefMod := ModOp Dividend Divisor Target`.
 *
 * @see Section 19.6.87 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_mod_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefAnd structure from the AML byte stream.
 *
 * The DefAnd structure is defined as `DefAnd := AndOp Operand Operand Target`.
 *
 * @see Section 19.6.5 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_and_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefNAnd structure from the AML byte stream.
 *
 * The DefNAnd structure is defined as `DefNAnd := NandOp Operand Operand Target`.
 *
 * @see Section 19.6.69 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_nand_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefOr structure from the AML byte stream.
 *
 * The DefOr structure is defined as `DefOr := OrOp Operand Operand Target`.
 *
 * @see Section 19.6.100 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_or_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefNOr structure from the AML byte stream.
 *
 * The DefNOr structure is defined as `DefNOr := NorOp Operand Operand Target`.
 *
 * @see Section 19.6.93 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_nor_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefXOr structure from the AML byte stream.
 *
 * The DefXOr structure is defined as `DefXOr := XorOp Operand Operand Target`.
 *
 * @see Section 19.6.155 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_xor_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefNot structure from the AML byte stream.
 *
 * The DefNot structure is defined as `DefNot := NotOp Operand Target`.
 *
 * @see Section 19.6.94 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_not_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a ShiftCount structure from the AML byte stream.
 *
 * A ShiftCount structure is defined as `ShiftCount := TermArg => Integer`.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the integer result will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_shift_count_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Reads a DefShiftLeft structure from the AML byte stream.
 *
 * The DefShiftLeft structure is defined as `DefShiftLeft := ShiftLeftOp Operand ShiftCount Target`.
 *
 * @see Section 19.6.123 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_shift_left_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefShiftRight structure from the AML byte stream.
 *
 * The DefShiftRight structure is defined as `DefShiftRight := ShiftRightOp Operand ShiftCount Target`.
 *
 * @see Section 19.6.124 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_shift_right_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefIncrement structure from the AML byte stream.
 *
 * The DefIncrement structure is defined as `DefIncrement := IncrementOp SuperName`.
 *
 * @see Section 19.6.62 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_increment_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefDecrement structure from the AML byte stream.
 *
 * The DefDecrement structure is defined as `DefDecrement := DecrementOp SuperName`.
 *
 * @see Section 19.6.27 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_decrement_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads an ObjReference structure from the AML byte stream.
 *
 * An ObjReference structure is defined as `ObjReference := TermArg => ObjectReference | String`.
 *
 * If a String is read then it is considered a path to an object and will be resolved to an ObjectReference.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_obj_reference_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefDerefOf structure from the AML byte stream.
 *
 * A DefDerefOf structure is defined as `DefDerefOf := DerefOfOp ObjReference`.
 *
 * @see Section 19.6.30 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_deref_of_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a BuffPkgStrObj structure from the AML byte stream.
 *
 * A BuffPkgStrObj structure is defined as `BuffPkgStrObj := TermArg => Buffer, Package, or String`.
 *
 * Note that the TermArg must resolve to an ObjectReference that points to a Buffer, Package, or String.
 * Becouse taking a reference to an object within a temporary object does not make sense, temporary objects are not
 * allowed.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_buff_pkg_str_obj_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads an IndexValue structure from the AML byte stream.
 *
 * An IndexValue structure is defined as `IndexValue := TermArg => Integer`.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the integer result will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_index_value_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

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
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_index_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLAnd structure from the AML byte stream.
 *
 * A DefLAnd structure is defined as `DefLAnd := LandOp Operand Operand`.
 *
 * @see Section 19.6.69 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_land_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLEqual structure from the AML byte stream.
 *
 * A DefLEqual structure is defined as `DefLEqual := LequalOp Operand Operand`.
 *
 * @see Section 19.6.70 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_lequal_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLGreater structure from the AML byte stream.
 *
 * A DefLGreater structure is defined as `DefLGreater := LgreaterOp Operand Operand`.
 *
 * @see Section 19.6.71 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_lgreater_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLGreaterEqual structure from the AML byte stream.
 *
 * A DefLGreaterEqual structure is defined as `DefLGreaterEqual := LgreaterEqualOp Operand Operand`.
 *
 * @see Section 19.6.72 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_lgreater_equal_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLLess structure from the AML byte stream.
 *
 * A DefLLess structure is defined as `DefLLess := LlessOp Operand Operand`.
 *
 * @see Section 19.6.73 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_lless_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLLessEqual structure from the AML byte stream.
 *
 * A DefLLessEqual structure is defined as `DefLLessEqual := LlessEqualOp Operand Operand`.
 *
 * @see Section 19.6.74 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_lless_equal_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLNot structure from the AML byte stream.
 *
 * A DefLNot structure is defined as `DefLNot := LnotOp Operand`.
 *
 * @see Section 19.6.75 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_lnot_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLNotEqual structure from the AML byte stream.
 *
 * A DefLNotEqual structure is defined as `DefLNotEqual := LnotEqualOp Operand Operand`.
 *
 * @see Section 19.6.76 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_lnot_equal_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefLOr structure from the AML byte stream.
 *
 * A DefLOr structure is defined as `DefLOr := LorOp Operand Operand`.
 *
 * @see Section 19.6.80 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_lor_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a MutexObject structure from the AML byte stream.
 *
 * A MutexObject structure is defined as `MutexObject := SuperName`.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_mutex_object_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a Timeout structure from the AML byte stream.
 *
 * A Timeout structure is defined as `Timeout := WordData`.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the integer result will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_timeout_read(aml_term_list_ctx_t* ctx, uint16_t* out);

/**
 * @brief Reads a DefAcquire structure from the AML byte stream.
 *
 * A DefAcquire structure is defined as `DefAcquire := AcquireOp MutexObject Timeout`.
 *
 * @see Section 19.6.2 of the ACPI specification for more details.
 * @see Section 19.6.89 of the ACPI specification for details about SyncLevel handling.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_acquire_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefToBcd structure from the AML byte stream.
 *
 * A DefToBcd structure is defined as `DefToBCD := ToBCDOp Operand Target`.
 *
 * @see Section 19.6.137 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_to_bcd_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefToBuffer structure from the AML byte stream.
 *
 * A DefToBuffer structure is defined as `DefToBuffer := ToBufferOp Operand Target`.
 *
 * @see Section 19.6.138 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_to_buffer_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefToDecimalString structure from the AML byte stream.
 *
 * A DefToDecimalString structure is defined as `DefToDecimalString := ToDecimalStringOp Operand Target`.
 *
 * @see Section 19.6.139 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_to_decimal_string_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefToHexString structure from the AML byte stream.
 *
 * A DefToHexString structure is defined as `DefToHexString := ToHexStringOp Operand Target`.
 *
 * @see Section 19.6.140 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_to_hex_string_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefToInteger structure from the AML byte stream.
 *
 * A DefToInteger structure is defined as `DefToInteger := ToIntegerOp Operand Target`.
 *
 * @see Section 19.6.141 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_to_integer_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a LengthArg structure from the AML byte stream.
 *
 * A LengthArg structure is defined as `LengthArg := TermArg => Integer`.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the integer result will be stored.
 * @return On success, the integer value. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_length_arg_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Reads a DefToString structure from the AML byte stream.
 *
 * A DefToString structure is defined as `DefToString := ToStringOp TermArg LengthArg Target`.
 *
 * @see Section 19.6.143 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_to_string_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefTimer structure from the AML byte stream.
 *
 * A DefTimer structure is defined as `DefTimer := TimerOp`.
 *
 * @see Section 19.6.136 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_timer_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefCopyObject structure from the AML byte stream.
 *
 * A DefCopyObject structure is defined as `DefCopyObject := CopyObjectOp TermArg SimpleName`.
 *
 * @see Section 19.6.17 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_copy_object_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a Data structure from the AML byte stream.
 *
 * A Data structure is defined as `Data := TermArg => ComputationalData`.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_data_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefConcat structure from the AML byte stream.
 *
 * A DefConcat structure is defined as `DefConcat := ConcatOp Data Data Target`.
 *
 * @see Section 19.6.20 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_concat_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefSizeOf structure from the AML byte stream.
 *
 * A DefSizeOf structure is defined as `DefSizeOf := SizeOfOp SuperName`.
 *
 * @see Section 19.6.126 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_size_of_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefRefOf structure from the AML byte stream.
 *
 * A DefRefOf structure is defined as `DefRefOf := RefOfOp SuperName`.
 *
 * @see Section 19.6.115 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_ref_of_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefObjectType structure from the AML byte stream.
 *
 * A DefObjectType structure is defined as `DefObjectType := ObjectTypeOp <SimpleName | DebugObj | DefRefOf | DefDerefOf
 * | DefIndex>`.
 *
 * @see Section 19.6.97 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_object_type_read(aml_term_list_ctx_t* ctx);

/*
 * @brief Reads a ReferenceTypeOpcode structure from the AML byte stream.
 *
 * A ReferenceTypeOpcode structure is defined as `ReferenceTypeOpcode := DefRefOf | DefDerefOf | DefIndex |
 * UserTermObj`.
 *
 * I have no idea what the `UserTermObj` is supposed to be, so its currently unimplemented.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_reference_type_opcode_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefFindSetLeftBit structure from the AML byte stream.
 *
 * A DefFindSetLeftBit structure is defined as `DefFindSetLeftBit := FindSetLeftBitOp Operand Target`.
 *
 * @see Section 19.6.49 the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_find_set_left_bit_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefFindSetRightBit structure from the AML byte stream.
 *
 * A DefFindSetRightBit structure is defined as `DefFindSetRightBit := FindSetRightBitOp Operand Target`.
 *
 * @see Section 19.6.50 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_find_set_right_bit_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a SearchPkg structure from the AML byte stream.
 *
 * A SearchPkg structure is defined as `SearchPkg := TermArg => Package`.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_package_obj_t* aml_search_pkg_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Match opcodes for DefMatch.
 * @enum aml_match_opcode_t
 */
typedef enum
{
    AML_MATCH_MTR = 0,
    AML_MATCH_MEQ = 1,
    AML_MATCH_MLE = 2,
    AML_MATCH_MLT = 3,
    AML_MATCH_MGE = 4,
    AML_MATCH_MGT = 5,
} aml_match_opcode_t;

/**
 * @brief Reads a MatchOpcode structure from the AML byte stream.
 *
 * A MatchOpcode structure is defined as `MatchOpcode :=
 *   ByteData // 0 MTR
 *   // 1 MEQ
 *   // 2 MLE
 *   // 3 MLT
 *   // 4 MGE
 *   // 5 MGT`.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the match opcode will be stored.
 * @return On success, the match opcode. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_match_opcode_read(aml_term_list_ctx_t* ctx, aml_match_opcode_t* out);

/**
 * @brief Reads a StartIndex structure from the AML byte stream.
 *
 * A StartIndex structure is defined as `StartIndex := TermArg => Integer`.
 *
 * @param ctx The TermList context.
 * @param out Output pointer where the integer result will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_start_index_read(aml_term_list_ctx_t* ctx, aml_integer_t* out);

/**
 * @brief Reads a DefMatch structure from the AML byte stream.
 *
 * A DefMatch structure is defined as `DefMatch := MatchOp SearchPkg MatchOpcode Operand MatchOpcode Operand
 * StartIndex`.
 *
 * @see Section 19.6.81 of the ACPI specification for more details.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_def_match_read(aml_term_list_ctx_t* ctx);

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
 * Currently unimplemented Opcodes are:
 * - `DefConcatRes`
 * - `DefFromBCD`
 * - `DefMid`
 * - `DefLoadTable`
 * - `DefWait`
 *
 * The result may have the `AML_OBJECT_EXCEPTION_ON_USE` flag set.
 *
 * @param ctx The TermList context.
 * @return On success, the object pointer storing the result. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_expression_opcode_read(aml_term_list_ctx_t* ctx);

/** @} */

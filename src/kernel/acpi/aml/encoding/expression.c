#include "expression.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "acpi/aml/runtime/evaluate.h"
#include "acpi/aml/runtime/method.h"
#include "arg.h"
#include "package_length.h"
#include "term.h"

uint64_t aml_buffer_size_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_buffer_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t bufferOp;
    if (aml_value_read(state, &bufferOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BufferOp");
        return ERR;
    }

    if (bufferOp.num != AML_BUFFER_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid BufferOp '0x%x'", bufferOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLength");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    uint64_t bufferSize;
    if (aml_buffer_size_read(state, scope, &bufferSize) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BufferSize");
        return ERR;
    }

    uint64_t availableBytes = (uint64_t)(end - state->current);

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        return ERR;
    }

    if (aml_node_init_buffer(*out, state->current, availableBytes, bufferSize) == ERR)
    {
        aml_node_deinit(*out);
        return ERR;
    }

    aml_state_advance(state, availableBytes);
    return 0;
}

uint64_t aml_term_arg_list_read(aml_state_t* state, aml_scope_t* scope, uint64_t argCount, aml_term_arg_list_t* out)
{
    if (argCount > AML_MAX_ARGS)
    {
        errno = EILSEQ;
        return ERR;
    }

    out->count = 0;
    for (uint64_t i = 0; i < argCount; i++)
    {
        out->args[i] = AML_NODE_CREATE;

        aml_node_t* termArg = NULL;
        if (aml_term_arg_read(state, scope, &termArg, AML_DATA_ALL) == ERR ||
            aml_node_clone(termArg, &out->args[i]) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_deinit(&out->args[j]);
            }
            return ERR;
        }

        out->count++;
    }

    return 0;
}

uint64_t aml_method_invocation_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_name_string_t nameString;
    aml_node_t* target = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve name string");
        return ERR;
    }

    if (target->type == AML_DATA_METHOD)
    {
        aml_term_arg_list_t args = {0};
        if (aml_term_arg_list_read(state, scope, target->method.flags.argCount, &args) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read method arguments");
            return ERR;
        }

        if (aml_scope_ensure_node(scope, out) == ERR)
        {
            return ERR;
        }

        if (aml_method_evaluate(target, &args, *out) == ERR)
        {
            aml_node_deinit(*out);
            for (uint64_t i = 0; i < args.count; i++)
            {
                aml_node_deinit(&args.args[i]);
            }
            return ERR;
        }

        for (uint64_t i = 0; i < args.count; i++)
        {
            aml_node_deinit(&args.args[i]);
        }

        return 0;
    }

    if (*out == NULL)
    {
        *out = target;
        return 0;
    }

    if (aml_evaluate(target, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        return ERR;
    }

    return 0;
}

uint64_t aml_def_cond_ref_of_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t condRefOfOp;
    if (aml_value_read(state, &condRefOfOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read CondRefOfOp");
        return ERR;
    }

    if (condRefOfOp.num != AML_COND_REF_OF_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid CondRefOfOp '0x%x'", condRefOfOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* source = NULL;
    if (aml_super_name_read_and_resolve(state, scope, &source, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    aml_node_t* result = NULL;
    if (aml_target_read_and_resolve(state, scope, &result, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        return ERR;
    }

    if (source == NULL)
    {
        // Return false since the source did not resolve to an object.
        if (aml_node_init_integer(*out, 0) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init false integer");
            return ERR;
        }
        return 0;
    }

    if (result == NULL)
    {
        // Return true since source resolved to an object and result dident so we dont need to store anything.
        if (aml_node_init_integer(*out, 1) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init true integer");
            return ERR;
        }
        return 0;
    }

    // Store a reference to source in the result and return true.

    if (aml_node_init_object_reference(result, source) == ERR)
    {
        aml_node_deinit(*out);
        AML_DEBUG_ERROR(state, "Failed to init ObjectReference in result");
        return ERR;
    }

    if (aml_node_init_integer(*out, 1) == ERR)
    {
        aml_node_deinit(*out);
        AML_DEBUG_ERROR(state, "Failed to init true integer");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_store_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t storeOp;
    if (aml_value_read(state, &storeOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read StoreOp");
        return ERR;
    }

    if (storeOp.num != AML_STORE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid StoreOp '0x%x'", storeOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* source = NULL;
    if (aml_term_arg_read(state, scope, &source, AML_DATA_ALL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    aml_node_t* destination = NULL;
    if (aml_super_name_read_and_resolve(state, scope, &destination, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    if (aml_evaluate(source, destination, AML_DATA_ALL) == ERR)
    {
        return ERR;
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        return ERR;
    }

    if (aml_evaluate(source, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        return ERR;
    }

    return 0;
}

uint64_t aml_operand_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out, aml_data_type_t allowedTypes)
{
    if (aml_term_arg_read(state, scope, out, allowedTypes) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_dividend_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_divisor_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_remainder_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    if (aml_target_read_and_resolve(state, scope, out, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }
    return 0;
}

uint64_t aml_quotient_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    if (aml_target_read_and_resolve(state, scope, out, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_add_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t addOp;
    if (aml_value_read(state, &addOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read AddOp");
        return ERR;
    }

    if (addOp.num != AML_ADD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid AddOp '0x%x'", addOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    aml_node_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, operand1->integer.value + operand2->integer.value) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_subtract_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t subtractOp;
    if (aml_value_read(state, &subtractOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read SubtractOp");
        return ERR;
    }

    if (subtractOp.num != AML_SUBTRACT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid SubtractOp '0x%x'", subtractOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    aml_node_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, operand1->integer.value - operand2->integer.value) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_multiply_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t multiplyOp;
    if (aml_value_read(state, &multiplyOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read MultiplyOp");
        return ERR;
    }

    if (multiplyOp.num != AML_MULTIPLY_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid MultiplyOp '0x%x'", multiplyOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    aml_node_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, operand1->integer.value * operand2->integer.value) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_divide_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t divOp;
    if (aml_value_read_no_ext(state, &divOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DivideOp");
        return ERR;
    }

    if (divOp.num != AML_DIVIDE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid DivideOp '0x%x'", divOp.num);
        errno = EILSEQ;
        return ERR;
    }

    uint64_t result = ERR;

    uint64_t dividend;
    if (aml_dividend_read(state, scope, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Dividend");
        return ERR;
    }

    uint64_t divisor;
    if (aml_divisor_read(state, scope, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Divisor");
        return ERR;
    }

    if (divisor == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero");
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* remainderDest = NULL;
    if (aml_remainder_read(state, scope, &remainderDest) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read remainder");
        return ERR;
    }

    aml_node_t* quotientDest = NULL;
    if (aml_quotient_read(state, scope, &quotientDest) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read quotient");
        return ERR;
    }

    aml_node_t remainder = AML_NODE_CREATE;
    if (aml_node_init_integer(&remainder, dividend % divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init remainder");
        return ERR;
    }

    aml_node_t quotient = AML_NODE_CREATE;
    if (aml_node_init_integer(&quotient, dividend / divisor) == ERR)
    {
        aml_node_deinit(&remainder);
        AML_DEBUG_ERROR(state, "Failed to init quotient");
        return ERR;
    }

    if (quotientDest == NULL)
    {
        if (aml_evaluate(&quotient, quotientDest, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&remainder);
            aml_node_deinit(&quotient);
            return ERR;
        }
    }

    if (remainderDest != NULL)
    {
        if (aml_evaluate(&remainder, remainderDest, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&remainder);
            aml_node_deinit(&quotient);
            return ERR;
        }
    }

    aml_node_deinit(&remainder);

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&quotient);
        return ERR;
    }

    if (aml_evaluate(&quotient, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&quotient);
        return ERR;
    }

    aml_node_deinit(&quotient);
    return 0;
}

uint64_t aml_def_mod_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t modOp;
    if (aml_value_read(state, &modOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ModOp");
        return ERR;
    }

    if (modOp.num != AML_MOD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ModOp '0x%x'", modOp.num);
        errno = EILSEQ;
        return ERR;
    }

    uint64_t dividend;
    if (aml_dividend_read(state, scope, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Dividend");
        return ERR;
    }

    uint64_t divisor;
    if (aml_divisor_read(state, scope, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Divisor");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    if (divisor == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero");
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, dividend % divisor) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_and_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t andOp;
    if (aml_value_read(state, &andOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read AndOp");
        return ERR;
    }

    if (andOp.num != AML_AND_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid AndOp '0x%x'", andOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    aml_node_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, operand1->integer.value & operand2->integer.value) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_INTEGER) == ERR)
    {
        aml_node_deinit(&result);
        aml_node_deinit(*out);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_nand_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t nandOp;
    if (aml_value_read(state, &nandOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (nandOp.num != AML_NAND_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid nand op: 0x%x", nandOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    aml_node_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, ~(operand1->integer.value & operand2->integer.value)) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init result integer");
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result in target");
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_or_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t orOp;
    if (aml_value_read(state, &orOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (orOp.num != AML_OR_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid or op: 0x%x", orOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    aml_node_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, operand1->integer.value | operand2->integer.value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init result integer");
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result in target");
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_nor_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t norOp;
    if (aml_value_read(state, &norOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (norOp.num != AML_NOR_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid nor op: 0x%x", norOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    aml_node_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, ~(operand1->integer.value | operand2->integer.value)) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init result integer");
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result in target");
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_xor_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t xorOp;
    if (aml_value_read(state, &xorOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (xorOp.num != AML_XOR_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid xor op: 0x%x", xorOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    aml_node_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, operand1->integer.value ^ operand2->integer.value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init result integer");
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result in target");
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_not_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t notOp;
    if (aml_value_read(state, &notOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (notOp.num != AML_NOT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid not op: 0x%x", notOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, ~operand->integer.value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init result integer");
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result in target");
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_shift_count_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_shift_left_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t shlOp;
    if (aml_value_read(state, &shlOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftLeftOp");
        return ERR;
    }

    if (shlOp.num != AML_SHIFT_LEFT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ShiftLeftOp '0x%x'", shlOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return ERR;
    }

    uint64_t shiftCount;
    if (aml_shift_count_read(state, scope, &shiftCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftCount");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    // C will discard the most significant bits
    aml_node_t result = AML_NODE_CREATE;
    if (shiftCount >= sizeof(uint64_t) * 8)
    {
        if (aml_node_init_integer(&result, 0) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_node_init_integer(&result, operand->integer.value << shiftCount) == ERR)
        {
            return ERR;
        }
    }

    // Target is allowed to be null
    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result");
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_shift_right_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t shrOp;
    if (aml_value_read(state, &shrOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftRightOp");
        return ERR;
    }

    if (shrOp.num != AML_SHIFT_RIGHT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ShiftRightOp '0x%x'", shrOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return ERR;
    }

    uint64_t shiftCount;
    if (aml_shift_count_read(state, scope, &shiftCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftCount");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    // C will zero the most significant bits
    aml_node_t result = AML_NODE_CREATE;
    if (shiftCount >= sizeof(uint64_t) * 8)
    {
        if (aml_node_init_integer(&result, 0) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_node_init_integer(&result, operand->integer.value >> shiftCount) == ERR)
        {
            return ERR;
        }
    }

    // Target is allowed to be null
    if (target != NULL)
    {
        if (aml_evaluate(&result, target, AML_DATA_ALL) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result");
            return ERR;
        }
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&result);
        return ERR;
    }

    if (aml_evaluate(&result, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&result);
        return ERR;
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_def_increment_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t incOp;
    if (aml_value_read(state, &incOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IncrementOp");
        return ERR;
    }

    if (incOp.num != AML_INCREMENT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid IncrementOp '0x%x'", incOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* addend;
    if (aml_super_name_read_and_resolve(state, scope, &addend, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    aml_node_t value = AML_NODE_CREATE;
    if (aml_evaluate(addend, &value, AML_DATA_INTEGER) == ERR)
    {
        return ERR;
    }

    value.integer.value++;

    if (aml_evaluate(&value, addend, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(&value);
        return ERR;
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&value);
        return ERR;
    }

    if (aml_evaluate(&value, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&value);
        return ERR;
    }

    aml_node_deinit(&value);
    return 0;
}

uint64_t aml_def_decrement_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t decOp;
    if (aml_value_read(state, &decOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DecrementOp");
        return ERR;
    }

    if (decOp.num != AML_DECREMENT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid DecrementOp '0x%x'", decOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* subtrahend;
    if (aml_super_name_read_and_resolve(state, scope, &subtrahend, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    aml_node_t value = AML_NODE_CREATE;
    if (aml_evaluate(subtrahend, &value, AML_DATA_INTEGER) == ERR)
    {
        return ERR;
    }

    assert(value.type == AML_DATA_INTEGER);

    value.integer.value--;

    if (aml_evaluate(&value, subtrahend, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(&value);
        return ERR;
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(&value);
        return ERR;
    }

    if (aml_evaluate(&value, *out, AML_DATA_ALL) == ERR)
    {
        aml_node_deinit(*out);
        aml_node_deinit(&value);
        return ERR;
    }

    aml_node_deinit(&value);
    return 0;
}

uint64_t aml_obj_reference_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_node_t* termArg = NULL;
    if (aml_term_arg_read(state, scope, &termArg, AML_DATA_OBJECT_REFERENCE | AML_DATA_STRING) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    if (termArg->type == AML_DATA_OBJECT_REFERENCE)
    {
        *out = termArg->objectReference.target;
        return 0;
    }
    else if (termArg->type == AML_DATA_STRING)
    {
        aml_node_t* target = aml_node_find(scope->node, termArg->string.content);
        if (target == NULL)
        {
            AML_DEBUG_ERROR(state, "Failed to find target scope '%s'", termArg->string.content);
            errno = EILSEQ;
            return ERR;
        }

        *out = target;
        return 0;
    }

    // Should never happen.
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_def_deref_of_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t derefOfOp;
    if (aml_value_read(state, &derefOfOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DerefOfOp");
        return ERR;
    }

    if (derefOfOp.num != AML_DEREF_OF_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid DerefOfOp '0x%x'", derefOfOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* obj = NULL;
    if (aml_obj_reference_read(state, scope, &obj) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ObjReference");
        return ERR;
    }

    if (obj == NULL)
    {
        AML_DEBUG_ERROR(state, "ObjReference is a null reference");
        errno = EILSEQ;
        return ERR;
    }

    *out = obj;
    return 0;
}

uint64_t aml_buff_pkg_str_obj_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    if (aml_term_arg_read(state, scope, out, AML_DATA_BUFFER | AML_DATA_PACKAGE | AML_DATA_STRING) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_index_value_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_index_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t indexOp;
    if (aml_value_read(state, &indexOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IndexOp");
        return ERR;
    }

    if (indexOp.num != AML_INDEX_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid IndexOp '0x%x'", indexOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* bufferPkgStrObj = NULL;
    if (aml_buff_pkg_str_obj_read(state, scope, &bufferPkgStrObj) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BuffPkgStrObj");
        return ERR;
    }

    uint64_t index;
    if (aml_index_value_read(state, scope, &index) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IndexValue");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        return ERR;
    }

    switch (bufferPkgStrObj->type)
    {
    case AML_DATA_PACKAGE: // Section 19.6.63.1
    {
        if (index >= bufferPkgStrObj->package.length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for package");
            errno = EILSEQ;
            return ERR;
        }

        if (aml_node_init_object_reference(*out, bufferPkgStrObj->package.elements[index]) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for package element");
            return ERR;
        }
    }
    break;
    case AML_DATA_BUFFER: // Section 19.6.63.2
    {
        if (index >= bufferPkgStrObj->buffer.length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for buffer");
            errno = EILSEQ;
            return ERR;
        }

        if (aml_node_init_object_reference(*out, &bufferPkgStrObj->buffer.byteFields[index]) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for buffer");
            return ERR;
        }
    }
    break;
    case AML_DATA_STRING: // Section 19.6.63.3
    {
        if (index >= bufferPkgStrObj->string.length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for string");
            errno = EILSEQ;
            return ERR;
        }

        if (aml_node_init_object_reference(*out, &bufferPkgStrObj->string.byteFields[index]) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for string");
            return ERR;
        }
    }
    break;
    default:
        AML_DEBUG_ERROR(state, "Invalid type, expected buffer, package or string but got '%s'",
            aml_data_type_to_string(bufferPkgStrObj->type));
        errno = EILSEQ;
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_node_clone(*out, target) == ERR)
        {
            aml_node_deinit(*out);
            AML_DEBUG_ERROR(state, "Failed to clone result to target");
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_def_l_and_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_def_l_equal_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_def_l_greater_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_def_l_greater_equal_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_def_l_less_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_def_l_less_equal_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_def_l_not_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_def_l_not_equal_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_def_l_or_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    (void)state;
    (void)scope;
    (void)out;
    errno = ENOSYS;
    return ERR;
}

uint64_t aml_expression_opcode_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t op;
    if (aml_value_peek(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    if (op.props->type == AML_VALUE_TYPE_NAME)
    {
        if (aml_method_invocation_read(state, scope, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read MethodInvocation");
            return ERR;
        }
        return 0;
    }

    uint64_t result = 0;
    switch (op.num)
    {
    case AML_BUFFER_OP:
        result = aml_def_buffer_read(state, scope, out);
        break;
    case AML_COND_REF_OF_OP:
        result = aml_def_cond_ref_of_read(state, scope, out);
        break;
    case AML_STORE_OP:
        result = aml_def_store_read(state, scope, out);
        break;
    case AML_ADD_OP:
        result = aml_def_add_read(state, scope, out);
        break;
    case AML_SUBTRACT_OP:
        result = aml_def_subtract_read(state, scope, out);
        break;
    case AML_MULTIPLY_OP:
        result = aml_def_multiply_read(state, scope, out);
        break;
    case AML_DIVIDE_OP:
        result = aml_def_divide_read(state, scope, out);
        break;
    case AML_MOD_OP:
        result = aml_def_mod_read(state, scope, out);
        break;
    case AML_AND_OP:
        result = aml_def_and_read(state, scope, out);
        break;
    case AML_NAND_OP:
        result = aml_def_nand_read(state, scope, out);
        break;
    case AML_OR_OP:
        result = aml_def_or_read(state, scope, out);
        break;
    case AML_NOR_OP:
        result = aml_def_nor_read(state, scope, out);
        break;
    case AML_XOR_OP:
        result = aml_def_xor_read(state, scope, out);
        break;
    case AML_NOT_OP:
        result = aml_def_not_read(state, scope, out);
        break;
    case AML_SHIFT_LEFT_OP:
        result = aml_def_shift_left_read(state, scope, out);
        break;
    case AML_SHIFT_RIGHT_OP:
        result = aml_def_shift_right_read(state, scope, out);
        break;
    case AML_INCREMENT_OP:
        result = aml_def_increment_read(state, scope, out);
        break;
    case AML_DECREMENT_OP:
        result = aml_def_decrement_read(state, scope, out);
        break;
    case AML_DEREF_OF_OP:
        result = aml_def_deref_of_read(state, scope, out);
        break;
    case AML_INDEX_OP:
        result = aml_def_index_read(state, scope, out);
        break;
    default:
        AML_DEBUG_ERROR(state, "Unknown expression opcode '0x%x'", op.num);
        errno = ENOSYS;
        return ERR;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read opcode '%s'", op.props->name);
        return ERR;
    }

    return 0;
}

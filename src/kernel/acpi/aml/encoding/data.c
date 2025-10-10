#include "data.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_patch_up.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_token.h"
#include "acpi/aml/runtime/convert.h"
#include "expression.h"
#include "name.h"
#include "package_length.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_byte_data_read(aml_state_t* state, uint8_t* out)
{
    uint8_t byte;
    if (aml_state_read(state, &byte, 1) != 1)
    {
        AML_DEBUG_ERROR(state, "Failed to read byte data");
        errno = ENODATA;
        return ERR;
    }
    *out = byte;
    return 0;
}

uint64_t aml_word_data_read(aml_state_t* state, uint16_t* out)
{
    uint8_t byte1, byte2;
    if (aml_byte_data_read(state, &byte1) == ERR || aml_byte_data_read(state, &byte2) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read word data");
        return ERR;
    }
    *out = ((uint16_t)byte1) | (((uint16_t)byte2) << 8);
    return 0;
}

uint64_t aml_dword_data_read(aml_state_t* state, uint32_t* out)
{
    uint16_t word1, word2;
    if (aml_word_data_read(state, &word1) == ERR || aml_word_data_read(state, &word2) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read dword data");
        return ERR;
    }
    *out = ((uint32_t)word1) | (((uint32_t)word2) << 16);
    return 0;
}

uint64_t aml_qword_data_read(aml_state_t* state, uint64_t* out)
{
    uint32_t dword1, dword2;
    if (aml_dword_data_read(state, &dword1) == ERR || aml_dword_data_read(state, &dword2) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read qword data");
        return ERR;
    }
    *out = ((uint64_t)dword1) | (((uint64_t)dword2) << 32);
    return 0;
}

uint64_t aml_byte_const_read(aml_state_t* state, uint8_t* out)
{
    if (aml_token_expect(state, AML_BYTE_PREFIX) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read byte prefix");
        return ERR;
    }

    return aml_byte_data_read(state, out);
}

uint64_t aml_word_const_read(aml_state_t* state, uint16_t* out)
{
    if (aml_token_expect(state, AML_WORD_PREFIX) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read word prefix");
        return ERR;
    }

    return aml_word_data_read(state, out);
}

uint64_t aml_dword_const_read(aml_state_t* state, uint32_t* out)
{
    if (aml_token_expect(state, AML_DWORD_PREFIX) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read dword prefix");
        return ERR;
    }

    return aml_dword_data_read(state, out);
}

uint64_t aml_qword_const_read(aml_state_t* state, uint64_t* out)
{
    if (aml_token_expect(state, AML_QWORD_PREFIX) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read qword prefix");
        return ERR;
    }

    return aml_qword_data_read(state, out);
}

uint64_t aml_const_obj_read(aml_state_t* state, aml_object_t* out)
{
    aml_token_t token;
    if (aml_token_read(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ConstObj");
        return ERR;
    }

    switch (token.num)
    {
    case AML_ZERO_OP:
        if (aml_integer_set(out, 0) == ERR)
        {
            return ERR;
        }
        break;
    case AML_ONE_OP:
        if (aml_integer_set(out, 1) == ERR)
        {
            return ERR;
        }
        break;
    case AML_ONES_OP:
        if (aml_integer_set(out, aml_integer_ones()) == ERR)
        {
            return ERR;
        }
        break;
    default:
        AML_DEBUG_ERROR(state, "Invalid ConstObj token '0x%x'", token.num);
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_string_read(aml_state_t* state, aml_object_t* out)
{
    if (aml_token_expect(state, AML_STRING_PREFIX) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read StringPrefix");
        return ERR;
    }

    const char* start = (const char*)state->current;
    while (1)
    {
        uint8_t c;
        if (aml_state_read(state, &c, 1) != 1)
        {
            AML_DEBUG_ERROR(state, "Failed to read string character");
            errno = ENODATA;
            return ERR;
        }

        if (c == 0x00)
        {
            break;
        }

        if (c < 0x01 || c > 0x7F)
        {
            AML_DEBUG_ERROR(state, "Invalid ASCII character '0x%x' in string", c);
            errno = EILSEQ;
            return ERR;
        }
    }

    if (aml_string_set(out, start) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_revision_op_read(aml_state_t* state, aml_object_t* out)
{
    if (aml_token_expect(state, AML_REVISION_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read RevisionOp");
        return ERR;
    }

    if (aml_integer_set(out, AML_CURRENT_REVISION) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_computational_data_read(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek ComputationalData");
        return ERR;
    }

    uint64_t result = 0;
    switch (token.num)
    {
    case AML_BYTE_PREFIX:
    {
        uint8_t byte;
        if (aml_byte_const_read(state, &byte) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ByteConst");
            return ERR;
        }

        if (aml_integer_set(out, byte) == ERR)
        {
            return ERR;
        }
        return 0;
    }
    case AML_WORD_PREFIX:
    {
        uint16_t word;
        if (aml_word_const_read(state, &word) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read WordConst");
            return ERR;
        }

        if (aml_integer_set(out, word) == ERR)
        {
            return ERR;
        }
        return 0;
    }
    case AML_DWORD_PREFIX:
    {
        uint32_t dword;
        if (aml_dword_const_read(state, &dword) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read DWordConst");
            return ERR;
        }

        if (aml_integer_set(out, dword) == ERR)
        {
            return ERR;
        }
        return 0;
    }
    case AML_QWORD_PREFIX:
    {
        uint64_t qword;
        if (aml_qword_const_read(state, &qword) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read QWordConst");
            return ERR;
        }

        if (aml_integer_set(out, qword) == ERR)
        {
            return ERR;
        }
        return 0;
    }
    case AML_STRING_PREFIX:
        if (aml_string_read(state, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read String");
            return ERR;
        }
        return 0;
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
        if (aml_const_obj_read(state, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ConstObj");
            return ERR;
        }
        return 0;
    case AML_BUFFER_OP:
        if (aml_def_buffer_read(state, scope, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read Buffer");
            return ERR;
        }
        return 0;
    case AML_REVISION_OP:
        if (aml_revision_op_read(state, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read RevisionOp");
            return ERR;
        }
        return 0;
    default:
        AML_DEBUG_ERROR(state, "Invalid ComputationalData '%s' (0x%x)", token.props->name, token.num);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_num_elements_read(aml_state_t* state, uint8_t* out)
{
    if (aml_byte_data_read(state, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NumElements");
        return ERR;
    }

    return 0;
}

/**
 * Used to handle package elements that are names but mainly its used as a callback for
 * the @ref kernel_acpi_aml_patch_up system.
 */
static inline uint64_t aml_package_element_handle_name(aml_object_t* in, aml_object_t* out)
{
    if (in->type &
        (AML_INTEGER | AML_STRING | AML_BUFFER | AML_BUFFER_FIELD | AML_FIELD_UNIT |
            AML_PACKAGE)) // "... resolved to actual data by the AML interpreter"
    {
        // Unsure what the spec means by "actual data" but converting to DataObject seems to be the most sensible
        // interpretation.
        if (aml_convert_source(in, &out, AML_DATA_OBJECTS) == ERR)
        {
            LOG_ERR("failed to convert to data object in aml_package_element_handle_name()\n");
            return ERR;
        }
        return 0;
    }
    else // "... returned in the package as references"
    {
        if (aml_object_reference_set(out, in) == ERR)
        {
            LOG_ERR("failed to init ObjectReference in aml_package_element_handle_name()\n");
            return ERR;
        }
        return 0;
    }
}

uint64_t aml_package_element_read(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek PackageElement");
        return ERR;
    }

    if (token.props->type == AML_TOKEN_TYPE_NAME)
    {
        aml_name_string_t nameString;
        if (aml_name_string_read(state, &nameString) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read NameString");
            return ERR;
        }

        aml_object_t* object = aml_name_string_resolve(&nameString, scope->location);
        if (object == NULL)
        {
            if (aml_unresolved_set(out, &nameString, scope->location, aml_package_element_handle_name) == ERR)
            {
                return ERR;
            }
            return 0;
        }

        if (aml_package_element_handle_name(object, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to handle name in PackageElement");
            return ERR;
        }

        return 0;
    }

    if (aml_data_ref_object_read(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DataRefObject");
        return ERR;
    }

    return 0;
}

uint64_t aml_package_element_list_read(aml_state_t* state, aml_scope_t* scope, aml_package_obj_t* package,
    const uint8_t* end)
{
    uint64_t i = 0;
    while (state->current < end && i < package->length)
    {
        if (aml_package_element_read(state, scope, package->elements[i]) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_object_clear(package->elements[j]);
            }
            AML_DEBUG_ERROR(state, "Failed to read PackageElement %llu", i);
            return ERR;
        }
        i++;
    }

    return 0;
}

uint64_t aml_def_package_read(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    if (aml_token_expect(state, AML_PACKAGE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PackageOp");
        return ERR;
    }

    const uint8_t* start = state->current;

    // PkgLength specifies how many elements in the package are defined, others are left uninitialized.
    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLength");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    // NumElements specifies the capacity of the package.
    uint8_t numElements;
    if (aml_num_elements_read(state, &numElements) == ERR)
    {
        return ERR;
    }

    if (aml_package_set(out, numElements) == ERR)
    {
        return ERR;
    }

    if (aml_package_element_list_read(state, scope, &out->package, end) == ERR)
    {
        aml_object_clear(out);
        AML_DEBUG_ERROR(state, "Failed to read PackageElementList");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_var_num_elements_read(aml_state_t* state, aml_scope_t* scope, aml_integer_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg for VarNumElements");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_var_package_read(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    if (aml_token_expect(state, AML_VAR_PACKAGE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read VarPackageOp");
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

    uint64_t numElements;
    if (aml_def_var_num_elements_read(state, scope, &numElements) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read VarNumElements");
        return ERR;
    }

    if (aml_package_set(out, numElements) == ERR)
    {
        return ERR;
    }

    if (aml_package_element_list_read(state, scope, &out->package, end) == ERR)
    {
        aml_object_clear(out);
        AML_DEBUG_ERROR(state, "Failed to read PackageElementList");
        return ERR;
    }

    return 0;
}

uint64_t aml_data_object_read(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        return ERR;
    }

    uint64_t result = 0;
    switch (token.num)
    {
    case AML_PACKAGE_OP:
        result = aml_def_package_read(state, scope, out);
        break;
    case AML_VAR_PACKAGE_OP:
        result = aml_def_var_package_read(state, scope, out);
        break;
    default:
        result = aml_computational_data_read(state, scope, out);
        break;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", token.props->name);
        return ERR;
    }

    return 0;
}

uint64_t aml_data_ref_object_read(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    // TODO: Implement ObjectReference handling... somehow. I honestly have no clue what the spec wants you to do here.

    if (aml_data_object_read(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DataObject");
        return ERR;
    }

    return 0;
}

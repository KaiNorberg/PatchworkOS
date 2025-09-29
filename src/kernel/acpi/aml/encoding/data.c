#include "data.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_patch_up.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "acpi/aml/runtime/evaluate.h"
#include "data_integers.h"
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
    aml_value_t prefix;
    if (aml_value_read(state, &prefix) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (prefix.num != AML_BYTE_PREFIX)
    {
        AML_DEBUG_ERROR(state, "Invalid prefix for byte const: 0x%x", prefix.num);
        errno = EILSEQ;
        return ERR;
    }

    return aml_byte_data_read(state, out);
}

uint64_t aml_word_const_read(aml_state_t* state, uint16_t* out)
{
    aml_value_t prefix;
    if (aml_value_read(state, &prefix) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (prefix.num != AML_WORD_PREFIX)
    {
        AML_DEBUG_ERROR(state, "Invalid prefix for word const: 0x%x", prefix.num);
        errno = EILSEQ;
        return ERR;
    }

    return aml_word_data_read(state, out);
}

uint64_t aml_dword_const_read(aml_state_t* state, uint32_t* out)
{
    aml_value_t prefix;
    if (aml_value_read(state, &prefix) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (prefix.num != AML_DWORD_PREFIX)
    {
        AML_DEBUG_ERROR(state, "Invalid prefix for dword const: 0x%x", prefix.num);
        errno = EILSEQ;
        return ERR;
    }

    return aml_dword_data_read(state, out);
}

uint64_t aml_qword_const_read(aml_state_t* state, uint64_t* out)
{
    aml_value_t prefix;
    if (aml_value_read(state, &prefix) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (prefix.num != AML_QWORD_PREFIX)
    {
        AML_DEBUG_ERROR(state, "Invalid prefix for qword const: 0x%x", prefix.num);
        errno = EILSEQ;
        return ERR;
    }

    return aml_qword_data_read(state, out);
}

uint64_t aml_const_obj_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t value;
    if (aml_value_read_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ConstObj");
        return ERR;
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_ZERO_OP:
        if (aml_node_init_integer_constant(*out, 0) == ERR)
        {
            aml_node_deinit(*out);
            return ERR;
        }
        break;
    case AML_ONE_OP:
        if (aml_node_init_integer_constant(*out, 1) == ERR)
        {
            aml_node_deinit(*out);
            return ERR;
        }
        break;
    case AML_ONES_OP:
        if (aml_node_init_integer_constant(*out, ~0) == ERR)
        {
            aml_node_deinit(*out);
            return ERR;
        }
        break;
    default:
        AML_DEBUG_ERROR(state, "Invalid ConstObj value '0x%x'", value.num);
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_string_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t stringPrefix;
    if (aml_value_read(state, &stringPrefix) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read StringPrefix");
        return ERR;
    }

    if (stringPrefix.num != AML_STRING_PREFIX)
    {
        AML_DEBUG_ERROR(state, "Invalid StringPrefix '0x%x'", stringPrefix.num);
        errno = EILSEQ;
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

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        return ERR;
    }

    if (aml_node_init_string(*out, start) == ERR)
    {
        aml_node_deinit(*out);
        return ERR;
    }

    return 0;
}

uint64_t aml_computational_data_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek ComputationalData");
        return ERR;
    }

    uint64_t result = 0;
    switch (value.num)
    {
    case AML_BYTE_PREFIX:
    {
        uint8_t byte;
        result = aml_byte_const_read(state, &byte);
        if (result != ERR)
        {
            if (aml_scope_ensure_node(scope, out) == ERR)
            {
                return ERR;
            }
            if (aml_node_init_integer_constant(*out, byte) == ERR)
            {
                aml_node_deinit(*out);
                return ERR;
            }
        }
    }
    break;
    case AML_WORD_PREFIX:
    {
        uint16_t word;
        result = aml_word_const_read(state, &word);
        if (result != ERR)
        {
            if (aml_scope_ensure_node(scope, out) == ERR)
            {
                return ERR;
            }
            if (aml_node_init_integer_constant(*out, word) == ERR)
            {
                aml_node_deinit(*out);
                return ERR;
            }
        }
    }
    break;
    case AML_DWORD_PREFIX:
    {
        uint32_t dword;
        result = aml_dword_const_read(state, &dword);
        if (result != ERR)
        {
            if (aml_scope_ensure_node(scope, out) == ERR)
            {
                return ERR;
            }
            if (aml_node_init_integer_constant(*out, dword) == ERR)
            {
                aml_node_deinit(*out);
                return ERR;
            }
        }
    }
    break;
    case AML_QWORD_PREFIX:
    {
        uint64_t qword;
        result = aml_qword_const_read(state, &qword);
        if (result != ERR)
        {
            if (aml_scope_ensure_node(scope, out) == ERR)
            {
                return ERR;
            }
            if (aml_node_init_integer_constant(*out, qword) == ERR)
            {
                aml_node_deinit(*out);
                return ERR;
            }
        }
    }
    break;
    case AML_STRING_PREFIX:
        result = aml_string_read(state, scope, out);
        break;
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
        // TODO: Add revision handling
        result = aml_const_obj_read(state, scope, out);
        break;
    case AML_BUFFER_OP:
        result = aml_def_buffer_read(state, scope, out);
        break;
    default:
        errno = ENOSYS;
        AML_DEBUG_ERROR(state, "Invalid computational data value: 0x%x", value.num);
        return ERR;
        break;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", value.props->name);
        return ERR;
    }

    return 0;
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
static inline uint64_t aml_package_element_handle_name(aml_node_t* in, aml_node_t* out)
{
    aml_data_type_info_t* info = aml_data_type_get_info(in->type);
    if (info->flags & AML_DATA_FLAG_DATA_OBJECT) // "... resolved to actual data by the AML interpreter"
    {
        if (aml_evaluate(in, out, AML_DATA_ACTUAL_DATA) == ERR)
        {
            LOG_ERR("failed to convert to actual data in aml_package_element_handle_name()\n");
            return ERR;
        }
        return 0;
    }
    else if (info->flags & AML_DATA_FLAG_NON_DATA_OBJECT) // "... returned in the package as references"
    {
        if (aml_node_init_object_reference(out, in) == ERR)
        {
            LOG_ERR("failed to init ObjectReference in aml_package_element_handle_name()\n");
            return ERR;
        }
        return 0;
    }
    else
    {
        LOG_ERR("invalid data type '%s' in aml_package_element_handle_name()\n", info->name);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_package_element_read(aml_state_t* state, aml_scope_t* scope, aml_node_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek PackageElement");
        return ERR;
    }

    if (value.props->type == AML_VALUE_TYPE_NAME)
    {
        aml_name_string_t nameString;
        aml_node_t* namedReference = NULL;
        if (aml_simple_name_read_and_resolve(state, scope, &namedReference, AML_RESOLVE_ALLOW_UNRESOLVED,
                &nameString) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read or resolve SimpleName");
            return ERR;
        }

        // Evaulated to a valid namestring but could not be resolved.
        if (namedReference == NULL)
        {
            if (aml_node_init_unresolved(out, &nameString, scope->node, aml_package_element_handle_name) == ERR)
            {
                return ERR;
            }
            return 0;
        }

        if (aml_package_element_handle_name(namedReference, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to handle name in PackageElement");
            return ERR;
        }
        return 0;
    }
    else
    {
        if (aml_data_ref_object_read(state, scope, &out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read DataRefObject");
            return ERR;
        }

        return 0;
    }
}

uint64_t aml_package_element_list_read(aml_state_t* state, aml_scope_t* scope, aml_node_t* package, const uint8_t* end)
{
    uint64_t i = 0;
    while (state->current < end && i < package->package.length)
    {
        if (aml_package_element_read(state, scope, package->package.elements[i]) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_deinit(package->package.elements[j]);
            }
            return ERR;
        }
        i++;
    }

    return 0;
}

uint64_t aml_def_package_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t packageOp;
    if (aml_value_read(state, &packageOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PackageOp");
        return ERR;
    }

    if (packageOp.num != AML_PACKAGE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid PackageOp '0x%x'", packageOp.num);
        errno = EILSEQ;
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

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        return ERR;
    }

    if (aml_node_init_package(*out, numElements) == ERR)
    {
        return ERR;
    }

    if (aml_package_element_list_read(state, scope, *out, end) == ERR)
    {
        aml_node_deinit(*out);
        AML_DEBUG_ERROR(state, "Failed to read PackageElementList");
        return ERR;
    }

    return 0;
}

uint64_t aml_data_object_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    uint64_t result = 0;
    switch (value.num)
    {
    case AML_PACKAGE_OP:
        result = aml_def_package_read(state, scope, out);
        break;
    case AML_VAR_PACKAGE_OP:
        AML_DEBUG_ERROR(state, "Unimplemented DefVarPackage");
        errno = ENOSYS;
        return ERR;
    default:
        result = aml_computational_data_read(state, scope, out);
        break;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", value.props->name);
        return ERR;
    }

    return 0;
}

uint64_t aml_data_ref_object_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    // TODO: Implement ObjectReference handling... somehow. I honestly have no clue what the spec wants you to do here.

    if (aml_data_object_read(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DataObject");
        return ERR;
    }

    return 0;
}

#include "data.h"

#include "acpi/aml/runtime/evaluate.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_patch_up.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "data_integers.h"
#include "expression.h"
#include "name.h"
#include "package_length.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_byte_data_read(aml_state_t* state, aml_byte_data_t* out)
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

uint64_t aml_word_data_read(aml_state_t* state, aml_word_data_t* out)
{
    aml_byte_data_t byte1, byte2;
    if (aml_byte_data_read(state, &byte1) == ERR || aml_byte_data_read(state, &byte2) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read word data");
        return ERR;
    }
    *out = ((aml_word_data_t)byte1) | (((aml_word_data_t)byte2) << 8);
    return 0;
}

uint64_t aml_dword_data_read(aml_state_t* state, aml_dword_data_t* out)
{
    aml_word_data_t word1, word2;
    if (aml_word_data_read(state, &word1) == ERR || aml_word_data_read(state, &word2) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read dword data");
        return ERR;
    }
    *out = ((aml_dword_data_t)word1) | (((aml_dword_data_t)word2) << 16);
    return 0;
}

uint64_t aml_qword_data_read(aml_state_t* state, aml_qword_data_t* out)
{
    aml_dword_data_t dword1, dword2;
    if (aml_dword_data_read(state, &dword1) == ERR || aml_dword_data_read(state, &dword2) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read qword data");
        return ERR;
    }
    *out = ((aml_qword_data_t)dword1) | (((aml_qword_data_t)dword2) << 32);
    return 0;
}

uint64_t aml_byte_const_read(aml_state_t* state, aml_byte_const_t* out)
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

uint64_t aml_word_const_read(aml_state_t* state, aml_word_const_t* out)
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

uint64_t aml_dword_const_read(aml_state_t* state, aml_dword_const_t* out)
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

uint64_t aml_qword_const_read(aml_state_t* state, aml_qword_const_t* out)
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

uint64_t aml_const_obj_read(aml_state_t* state, aml_const_obj_t* out)
{
    aml_value_t value;
    if (aml_value_read_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    switch (value.num)
    {
    case AML_ZERO_OP:
        *out = 0;
        return 0;
    case AML_ONE_OP:
        *out = 1;
        return 0;
    case AML_ONES_OP:
        *out = ~0;
        return 0;
    default:
        AML_DEBUG_ERROR(state, "Invalid const obj value: 0x%x", value.num);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_string_read(aml_state_t* state, aml_node_t* out)
{
    aml_value_t stringPrefix;
    if (aml_value_read(state, &stringPrefix) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (stringPrefix.num != AML_STRING_PREFIX)
    {
        AML_DEBUG_ERROR(state, "Invalid prefix for string: 0x%x", stringPrefix.num);
        errno = EILSEQ;
        return ERR;
    }

    const char* str = (const char*)state->current;
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
            AML_DEBUG_ERROR(state, "Invalid string character: 0x%x", c);
            errno = EILSEQ;
            return ERR;
        }
    }

    if (aml_node_init_string(out, str) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init string node");
        return ERR;
    }
    return 0;
}

uint64_t aml_computational_data_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    switch (value.num)
    {
    case AML_BYTE_PREFIX:
    {
        aml_byte_const_t byte;
        if (aml_byte_const_read(state, &byte) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read byte const");
            return ERR;
        }
        if (aml_node_init_integer(out, byte) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init byte node");
            return ERR;
        }
        return 0;
    }
    case AML_WORD_PREFIX:
    {
        aml_word_const_t word;
        if (aml_word_const_read(state, &word) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read word const");
            return ERR;
        }
        if (aml_node_init_integer(out, word) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init word node");
            return ERR;
        }
        return 0;
    }
    case AML_DWORD_PREFIX:
    {
        aml_dword_const_t dword;
        if (aml_dword_const_read(state, &dword) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read dword const");
            return ERR;
        }
        if (aml_node_init_integer(out, dword) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init dword node");
            return ERR;
        }
        return 0;
    }
    case AML_QWORD_PREFIX:
    {
        aml_qword_const_t qword;
        if (aml_qword_const_read(state, &qword) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read qword const");
            return ERR;
        }
        if (aml_node_init_integer(out, qword) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init qword node");
            return ERR;
        }
        return 0;
    }
    case AML_STRING_PREFIX:
    {
        if (aml_string_read(state, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read buffer");
            return ERR;
        }
        return 0;
    }
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
    {
        // TODO: Add revision handling
        aml_const_obj_t constObj;
        if (aml_const_obj_read(state, &constObj) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read const obj");
            return ERR;
        }
        if (aml_node_init_integer_constant(out, constObj) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init const obj");
            return ERR;
        }
        return 0;
    }
    case AML_BUFFER_OP:
    {
        if (aml_def_buffer_read(state, node, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read buffer");
            return ERR;
        }
        return 0;
    }
    default:
        AML_DEBUG_ERROR(state, "Invalid computational data value: 0x%x", value.num);
        errno = ENOSYS;
        return ERR;
    }
}

uint64_t aml_num_elements_read(aml_state_t* state, aml_byte_data_t* out)
{
    return aml_byte_data_read(state, out);
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
        if (aml_evaluate_to_actual_data(in, out) == ERR)
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

uint64_t aml_package_element_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    if (value.props->type == AML_VALUE_TYPE_NAME)
    {
        aml_name_string_t nameString;
        aml_node_t* namedReference = NULL;
        if (aml_simple_name_read_and_resolve(state, node, &namedReference, AML_RESOLVE_ALLOW_UNRESOLVED, &nameString) ==
            ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read and resolve named reference");
            return ERR;
        }

        // Evaulated to a valid namestring but could not be resolved.
        if (namedReference == NULL)
        {
            if (aml_node_init_unresolved(out, &nameString, node) == ERR)
            {
                AML_DEBUG_ERROR(state, "Failed to init unresolved node");
                return ERR;
            }

            if (aml_patch_up_add_unresolved(out, aml_package_element_handle_name) == ERR)
            {
                AML_DEBUG_ERROR(state, "Failed to add to patch-up system");
                return ERR;
            }
            return 0;
        }

        if (aml_package_element_handle_name(namedReference, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to handle package element name");
            return ERR;
        }
        return 0;
    }
    else
    {
        if (aml_data_object_read(state, node, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read data object");
            return ERR;
        }
        return 0;
    }
}

uint64_t aml_package_element_list_read(aml_state_t* state, aml_node_t* node, aml_node_t* package, const uint8_t* end)
{
    if (package == NULL || package->type != AML_DATA_PACKAGE)
    {
        AML_DEBUG_ERROR(state, "Invalid package node");
        errno = EINVAL;
        return ERR;
    }

    uint64_t i = 0;
    while (state->current < end && i < package->package.length)
    {
        if (aml_package_element_read(state, node, package->package.elements[i]) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_deinit(package->package.elements[j]);
            }
            AML_DEBUG_ERROR(state, "Failed to read package element at index %llu", i);
            return ERR;
        }
        i++;
    }

    return 0;
}

uint64_t aml_def_package_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t packageOp;
    if (aml_value_read(state, &packageOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (packageOp.num != AML_PACKAGE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid package op: 0x%x", packageOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    // PkgLength specifies how many elements in the package are defined, others are left uninitialized.
    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pkg length");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    // NumElements specifies the capacity of the package.
    aml_byte_data_t numElements;
    if (aml_num_elements_read(state, &numElements) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read num elements");
        return ERR;
    }

    if (aml_node_init_package(out, numElements) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init package node with %u elements", numElements);
        return ERR;
    }

    if (aml_package_element_list_read(state, node, out, end) == ERR)
    {
        aml_node_deinit(out);
        AML_DEBUG_ERROR(state, "Failed to read package element list");
        return ERR;
    }

    return 0;
}

uint64_t aml_data_object_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    switch (value.num)
    {
    case AML_PACKAGE_OP:
    {
        if (aml_def_package_read(state, node, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read package");
            return ERR;
        }
        return 0;
    }
    case AML_VAR_PACKAGE_OP:
        AML_DEBUG_ERROR(state, "Unsupported var package op");
        errno = ENOSYS;
        return ERR;
    default:
        if (aml_computational_data_read(state, node, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read computational data");
            return ERR;
        }
        return 0;
    }
}

uint64_t aml_data_ref_object_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    // TODO: Implement ObjectReference handling

    return aml_data_object_read(state, node, out);
}

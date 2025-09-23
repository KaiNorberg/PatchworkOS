#include "data.h"

#include "acpi/aml/aml_convert.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "expression.h"
#include "mem/heap.h"
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

uint64_t aml_string_read(aml_state_t* state, aml_string_t* out)
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

    char* str = (char*)((uint64_t)state->data + (uint64_t)state->pos);
    uint64_t length = 0;
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

        length++;
    }

    out->content = str;
    out->length = length;
    out->inPlace = true;
    return 0;
}

uint64_t aml_computational_data_read(aml_state_t* state, aml_node_t* out)
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
        if (aml_node_init_integer(out, byte, 8) == ERR)
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
        if (aml_node_init_integer(out, word, 16) == ERR)
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
        if (aml_node_init_integer(out, dword, 32) == ERR)
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
        if (aml_node_init_integer(out, qword, 64) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init qword node");
            return ERR;
        }
        return 0;
    }
    case AML_STRING_PREFIX:
    {
        aml_string_t str;
        if (aml_string_read(state, &str) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read string");
            return ERR;
        }

        if (aml_node_init_string(out, str.content, str.inPlace) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init string");
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
        aml_buffer_t buffer;
        if (aml_def_buffer_read(state, &buffer) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read buffer");
            return ERR;
        }
        if (aml_node_init_buffer(out, buffer.content, buffer.length, buffer.capacity, buffer.inPlace) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init buffer");
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
        if (aml_name_string_read(state, &nameString) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read name string");
            return ERR;
        }

        aml_node_t* namedReference = aml_node_find(&nameString, node);
        if (namedReference == NULL)
        {
            AML_DEBUG_ERROR(state, "Failed to find named reference '%s'", aml_name_string_to_string(&nameString));
            errno = ENOENT;
            return ERR;
        }

        aml_data_type_info_t* info = aml_data_type_get_info(namedReference->type);
        if (info->flags & AML_DATA_FLAG_DATA_OBJECT) // "... resolved to actual data by the AML interpreter"
        {
            if (aml_convert_to_actual_data(namedReference, out) == ERR)
            {
                AML_DEBUG_ERROR(state, "Failed to convert named reference '%s' to actual data",
                    aml_name_string_to_string(&nameString));
                return ERR;
            }
            return 0;
        }
        else if (info->flags & AML_DATA_FLAG_NON_DATA_OBJECT) // "... returned in the package as references"
        {
            if (aml_node_init_object_reference(out, namedReference) == ERR)
            {
                AML_DEBUG_ERROR(state, "Failed to init object reference for named reference '%s'",
                    aml_name_string_to_string(&nameString));
                return ERR;
            }
            return 0;
        }
        else
        {
            AML_DEBUG_ERROR(state, "Named reference '%s' is neither a data object nor a non-data object",
                aml_name_string_to_string(&nameString));
            errno = EILSEQ;
            return ERR;
        }
    }
    else
    {
        return aml_data_ref_object_read(state, node, out);
    }
}

uint64_t aml_package_element_list_read(aml_state_t* state, aml_node_t* node, aml_node_t* package, aml_address_t end)
{
    if (package == NULL || package->type != AML_DATA_PACKAGE)
    {
        AML_DEBUG_ERROR(state, "Invalid package node");
        errno = EINVAL;
        return ERR;
    }

    uint64_t i = 0;
    while (state->pos < end && i < package->package.capacity)
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

    aml_address_t start = state->pos;

    // PkgLength specifies how many elements in the package are defined, others are left uninitialized.
    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pkg length");
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    // NumElements specifies the capacity of the package.
    aml_byte_data_t numElements;
    if (aml_num_elements_read(state, &numElements) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read num elements");
        return ERR;
    }

    if (aml_node_init_package(out, numElements) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init package node");
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
        if (aml_computational_data_read(state, out) == ERR)
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

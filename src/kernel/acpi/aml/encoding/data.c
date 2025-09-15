#include "data.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
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
        AML_DEBUG_INVALID_STRUCTURE("ByteData");
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
        AML_DEBUG_INVALID_STRUCTURE("WordData");
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
        AML_DEBUG_INVALID_STRUCTURE("DWordData");
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
        AML_DEBUG_INVALID_STRUCTURE("QWordData");
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
        return ERR;
    }

    if (prefix.num != AML_BYTE_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("ByteConst");
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
        return ERR;
    }

    if (prefix.num != AML_WORD_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("WordConst");
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
        return ERR;
    }

    if (prefix.num != AML_DWORD_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("DWordConst");
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
        return ERR;
    }

    if (prefix.num != AML_QWORD_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("QWordConst");
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
        AML_DEBUG_INVALID_STRUCTURE("ConstObj");
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_string_read(aml_state_t* state, aml_string_t* out)
{
    aml_value_t stringPrefix;
    if (aml_value_read(state, &stringPrefix) == ERR)
    {
        return ERR;
    }

    if (stringPrefix.num != AML_STRING_PREFIX)
    {
        AML_DEBUG_INVALID_STRUCTURE("String");
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
            AML_DEBUG_INVALID_STRUCTURE("String: Unexpected end of stream");
            errno = ENODATA;
            return ERR;
        }

        if (c == 0x00)
        {
            break;
        }

        if (c < 0x01 || c > 0x7F)
        {
            AML_DEBUG_INVALID_STRUCTURE("String: Non-ASCII character encountered");
            errno = EILSEQ;
            return ERR;
        }

        length++;
    }

    out->content = str;
    out->length = length;
    return 0;
}

uint64_t aml_computational_data_read(aml_state_t* state, aml_computational_data_t* out)
{
    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_BYTE_PREFIX:
    {
        aml_byte_const_t byte;
        if (aml_byte_const_read(state, &byte) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = byte;
        out->meta.bitWidth = 8;
        return 0;
    }
    case AML_WORD_PREFIX:
    {
        aml_word_const_t word;
        if (aml_word_const_read(state, &word) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = word;
        out->meta.bitWidth = 16;
        return 0;
    }
    case AML_DWORD_PREFIX:
    {
        aml_dword_const_t dword;
        if (aml_dword_const_read(state, &dword) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = dword;
        out->meta.bitWidth = 32;
        return 0;
    }
    case AML_QWORD_PREFIX:
    {
        aml_qword_const_t qword;
        if (aml_qword_const_read(state, &qword) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = qword;
        out->meta.bitWidth = 64;
        return 0;
    }
    case AML_STRING_PREFIX:
        out->type = AML_DATA_STRING;
        return aml_string_read(state, &out->string);
    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:
    {
        // TODO: Add revision handling
        aml_const_obj_t constObj;
        if (aml_const_obj_read(state, &constObj) == ERR)
        {
            return ERR;
        }
        out->type = AML_DATA_INTEGER;
        out->integer = constObj;
        out->meta.bitWidth = 64;
        return 0;
    }
    case AML_BUFFER_OP:
        out->type = AML_DATA_BUFFER;
        return aml_def_buffer_read(state, &out->buffer);
    default:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    }
}

uint64_t aml_num_elements_read(aml_state_t* state, aml_num_elements_t* out)
{
    return aml_byte_data_read(state, out);
}

uint64_t aml_package_element_read(aml_state_t* state, aml_package_element_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    if (value.props->type == AML_VALUE_TYPE_NAME)
    {
        out->type = AML_DATA_NAME_STRING;
        return aml_name_string_read(state, &out->nameString);
    }
    else
    {
        return aml_data_ref_object_read(state, out);
    }
}

uint64_t aml_package_element_list_read(aml_state_t* state, aml_data_object_t** out, aml_num_elements_t numElements)
{
    aml_package_element_t* elements = heap_alloc(sizeof(aml_data_object_t) * numElements, HEAP_NONE);
    if (elements == NULL)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < numElements; i++)
    {
        if (aml_package_element_read(state, &elements[i]) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                if (elements[j].type == AML_DATA_PACKAGE)
                {
                    aml_package_free(&elements[j].package);
                }
            }
            heap_free(elements);
            return ERR;
        }
    }

    *out = elements;
    return 0;
}

uint64_t aml_def_package_read(aml_state_t* state, aml_package_t* out)
{
    aml_value_t packageOp;
    if (aml_value_read(state, &packageOp) == ERR)
    {
        return ERR;
    }

    if (packageOp.num != AML_PACKAGE_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&packageOp);
        errno = EILSEQ;
        return ERR;
    }

    // Read PkgLength but ignore it, honestly not sure why its even here.
    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    if (aml_num_elements_read(state, &out->numElements) == ERR)
    {
        return ERR;
    }

    if (aml_package_element_list_read(state, &out->elements, out->numElements) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_data_object_read(aml_state_t* state, aml_data_object_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_PACKAGE_OP:
        out->type = AML_DATA_PACKAGE;
        return aml_def_package_read(state, &out->package);
    case AML_VAR_PACKAGE_OP:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    default:
        return aml_computational_data_read(state, out);
    }
}

uint64_t aml_data_ref_object_read(aml_state_t* state, aml_data_object_t* out)
{
    // TODO: Implement ObjectReference handling

    return aml_data_object_read(state, out);
}

void aml_package_free(aml_package_t* package)
{
    if (package->elements != NULL)
    {
        for (uint64_t i = 0; i < package->numElements; i++)
        {
            if (package->elements[i].type == AML_DATA_PACKAGE)
            {
                aml_package_free(&package->elements[i].package);
            }
        }
        heap_free(package->elements);
        package->elements = NULL;
        package->numElements = 0;
    }
}

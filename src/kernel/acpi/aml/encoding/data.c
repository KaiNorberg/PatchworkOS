#include "data.h"
#include "acpi/aml/aml_debug.h"

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

uint64_t aml_byte_const_read(aml_state_t* state, aml_byte_data_t* out)
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

uint64_t aml_word_const_read(aml_state_t* state, aml_word_data_t* out)
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

uint64_t aml_dword_const_read(aml_state_t* state, aml_dword_data_t* out)
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

uint64_t aml_qword_const_read(aml_state_t* state, aml_qword_data_t* out)
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

uint64_t aml_computational_data_read(aml_state_t* state, aml_computational_data_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_BYTE_PREFIX:
        out->type = AML_COMPUTATIONAL_BYTE;
        return aml_byte_const_read(state, &out->byte);
    case AML_WORD_PREFIX:
        out->type = AML_COMPUTATIONAL_WORD;
        return aml_word_const_read(state, &out->word);
    case AML_DWORD_PREFIX:
        out->type = AML_COMPUTATIONAL_DWORD;
        return aml_dword_const_read(state, &out->dword);
    case AML_QWORD_PREFIX:
        out->type = AML_COMPUTATIONAL_QWORD;
        return aml_qword_const_read(state, &out->qword);
    default:
        AML_DEBUG_UNIMPLEMENTED_STRUCTURE("String | ConstObj | RevisionOp | DefBuffer");
        errno = ENOSYS;
        return ERR;
    }
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
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    case AML_VAR_PACKAGE_OP:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    default:
        out->isComputational = true;
        return aml_computational_data_read(state, &out->computational);
    }
}

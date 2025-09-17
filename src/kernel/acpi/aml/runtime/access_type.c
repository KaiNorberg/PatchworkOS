#include "access_type.h"

void aml_align_bits(aml_bit_size_t bits, aml_access_type_t accessType, aml_bit_size_t* out, aml_bit_size_t* remainder)
{
    if (out == NULL || remainder == NULL)
    {
        return;
    }

    aml_bit_size_t alignSize = 1;
    switch (accessType)
    {
    case AML_ACCESS_TYPE_BYTE:
        alignSize = 8;
        break;
    case AML_ACCESS_TYPE_WORD:
        alignSize = 16;
        break;
    case AML_ACCESS_TYPE_DWORD:
        alignSize = 32;
        break;
    case AML_ACCESS_TYPE_QWORD:
        alignSize = 64;
        break;
    default:
        *out = bits;
        *remainder = 0;
        return;
    }

    *out = bits - (bits % alignSize);
    *remainder = bits % alignSize;
}

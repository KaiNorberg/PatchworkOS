#include "access_type.h"

#include <errno.h>

static inline uint64_t aml_round_up_to_power_of_two(uint64_t x)
{
    if (x == 0)
    {
        return 1;
    }
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
}

uint64_t aml_get_access_size(aml_bit_size_t bitSize, aml_access_type_t accessType, aml_region_space_t regionSpace,
    aml_bit_size_t* out)
{
    switch (accessType)
    {
    case AML_ACCESS_TYPE_BYTE:
        return 8;
    case AML_ACCESS_TYPE_WORD:
        return 16;
    case AML_ACCESS_TYPE_DWORD:
        return 32;
    case AML_ACCESS_TYPE_QWORD:
        return 64;
    case AML_ACCESS_TYPE_ANY:
    {
        // Unsure about this one, the spec is not very clear. The only section that seems to attempt to define the
        // behaviour of AnyAcc is 19.6.48. My interpretation is that the access size can just be whatever we want.
        //
        // Its then reasonable to pick a power of two so we can access bytes, words, dwords or qwords. And to also limit
        // the maximum access size to 32 bits generally except for system memory opregions where 64 bit accesses are
        // allowed (since the kernel is 64bit).
        //
        // In short valid values are generally 8, 16 or 32 except for system memory where 64 is also valid. We then pick
        // the smallest valid value that is >= the field size.
        //
        // Other implementations such as Lai seem to do the same.

        aml_bit_size_t size = aml_round_up_to_power_of_two(bitSize);

        aml_bit_size_t maxAccessSize = 32;
        if (regionSpace == AML_REGION_SYSTEM_MEMORY)
        {
            maxAccessSize = 64;
        }

        if (size > maxAccessSize)
        {
            size = maxAccessSize;
        }

        if (size < 8)
        {
            size = 8;
        }

        *out = size;
        return 0;
    }
    default:
        errno = EINVAL;
        return ERR;
    }
}

uint64_t aml_align_bits(aml_bit_size_t bits, aml_bit_size_t accessSize, aml_bit_size_t* out, aml_bit_size_t* remainder)
{
    if (out == NULL || remainder == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (accessSize == 0)
    {
        *out = 0;
        *remainder = bits;
        return 0;
    }

    aml_bit_size_t aligned = bits - (bits % accessSize);
    *out = aligned;
    *remainder = bits - aligned;
    return 0;
}

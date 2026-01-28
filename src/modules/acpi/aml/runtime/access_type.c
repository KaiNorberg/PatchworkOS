#include <kernel/acpi/aml/runtime/access_type.h>

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

aml_bit_size_t aml_get_access_size(aml_bit_size_t bitSize, aml_access_type_t accessType, aml_region_space_t regionSpace)
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
        // behaviour of AnyAcc is 19.6.48. My interpretation is that the access size can just be whatever we want, which
        // seems strange considering the behaviour of WriteAsOnes and WriteAsZeroes.
        //
        // Either way, its then reasonable to pick a power of two so we can access using bytes, words, dwords or qwords.
        // And to also limit the maximum access size to 32 bits generally (as ports can output a max of 32 bits) except
        // for system memory opregions where 64 bit accesses are allowed (since the kernel is 64bit) but only if the
        // acpi revision >= 2.
        //
        // In short valid values are generally 8, 16 or 32 except for system memory where 64 is also valid (if acpi
        // revision >= 2). We then pick the smallest valid value that is >= the field size.
        //
        // Other implementations such as Lai seem to do the same.

        aml_bit_size_t size = aml_round_up_to_power_of_two(bitSize);

        aml_bit_size_t maxAccessSize = 32;
        if (regionSpace == AML_REGION_SYSTEM_MEMORY)
        {
            maxAccessSize = aml_integer_bit_size();
        }

        if (size > maxAccessSize)
        {
            size = maxAccessSize;
        }

        if (size < 8)
        {
            size = 8;
        }

        return size;
    }
    default:
        return 8;
    }
}

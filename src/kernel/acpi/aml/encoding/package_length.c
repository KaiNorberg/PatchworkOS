#include "package_length.h"

#include "acpi/aml/aml_state.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_pkg_length_read(aml_state_t* state)
{
    uint64_t pkgLeadByte = aml_byte_read(state);
    if (pkgLeadByte == ERR)
    {
        errno = ENODATA;
        return ERR;
    }

    uint8_t bytedataCount = (pkgLeadByte >> 6) & 0b11; // bits (7-6)

    // If no bytes follow, then the first 6 bits store the package length.
    if (bytedataCount == 0)
    {
        return pkgLeadByte & 0b111111;
    }

    // If more bytes follow, then bits 4 and 5 must be zero.
    if (pkgLeadByte & (1 << 4) || pkgLeadByte & (1 << 5))
    {
        errno = EILSEQ;
        return ERR;
    }

    // Bits 0 to 3 in pkgLeadByte becomes the least significant bits in the length, followed by the next bytes.
    aml_pkg_length_t length = (pkgLeadByte & 0b1111);
    for (uint8_t i = 0; i < bytedataCount; i++)
    {
        uint64_t byte = aml_byte_read(state);
        if (byte == ERR)
        {
            errno = ENODATA;
            return ERR;
        }
        length |= ((uint64_t)byte) << (i * 8 + 4);
    }

    // Output must not be greater than 2^28.
    if (length > (1ULL << 28))
    {
        errno = ERANGE;
        return ERR;
    }

    return length;
}

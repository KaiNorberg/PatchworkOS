#include "package_length.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "data.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_pkg_lead_byte_read(aml_state_t* state, aml_pkg_lead_byte_t* out)
{
    uint8_t pkgLeadByte;
    if (aml_state_read(state, &pkgLeadByte, 1) != 1)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLeadByte");
        errno = ENODATA;
        return ERR;
    }

    out->byteDataCount = (pkgLeadByte >> 6) & 0x03;   // bits (7-6)
    out->smallLengthBits = pkgLeadByte & 0x3F;        // bits (5-0)
    out->leastSignificantNybble = pkgLeadByte & 0x0F; // bits (3-0)

    // If more bytes follow, then bits 4 and 5 must be zero.
    if (out->byteDataCount != 0 && ((pkgLeadByte >> 4) & 0x03) != 0)
    {
        AML_DEBUG_ERROR(state, "Invalid PkgLeadByte '0x%x'", pkgLeadByte);
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_pkg_length_read(aml_state_t* state, aml_pkg_length_t* out)
{
    aml_pkg_lead_byte_t pkgLeadByte;
    if (aml_pkg_lead_byte_read(state, &pkgLeadByte) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLeadByte");
        return ERR;
    }

    // If no bytes follow, then the smallLengthBits store the package length.
    if (pkgLeadByte.byteDataCount == 0)
    {
        *out = pkgLeadByte.smallLengthBits;
        return 0;
    }

    // Bits 0 to 3 in pkgLeadByte becomes the least significant bits in the length, followed by the next bytes.
    aml_pkg_length_t length = pkgLeadByte.leastSignificantNybble;
    for (uint64_t i = 0; i < pkgLeadByte.byteDataCount; i++)
    {
        uint8_t byte;
        if (aml_byte_data_read(state, &byte) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ByteData");
            return ERR;
        }
        length |= ((uint64_t)byte) << (4 + i * 8);
    }

    // Output must not be greater than 2^28.
    if (length > (1ULL << 28))
    {
        AML_DEBUG_ERROR(state, "Package length out of range: %lu", length);
        errno = ERANGE;
        return ERR;
    }

    *out = length;
    return 0;
}

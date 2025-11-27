#include <modules/acpi/aml/encoding/package_length.h>

#include <modules/acpi/aml/debug.h>
#include <modules/acpi/aml/encoding/data.h>
#include <modules/acpi/aml/state.h>

#include <errno.h>
#include <stdint.h>

uint64_t aml_pkg_lead_byte_read(aml_term_list_ctx_t* ctx, aml_pkg_lead_byte_t* out)
{
    uint8_t pkgLeadByte;
    if (aml_byte_data_read(ctx, &pkgLeadByte) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return ERR;
    }

    out->byteDataCount = (pkgLeadByte >> 6) & 0x03;   // bits (7-6)
    out->smallLengthBits = pkgLeadByte & 0x3F;        // bits (5-0)
    out->leastSignificantNybble = pkgLeadByte & 0x0F; // bits (3-0)

    // If more bytes follow, then bits 4 and 5 must be zero.
    if (out->byteDataCount != 0 && ((pkgLeadByte >> 4) & 0x03) != 0)
    {
        AML_DEBUG_ERROR(ctx, "Invalid PkgLeadByte '0x%x'", pkgLeadByte);
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_pkg_length_read(aml_term_list_ctx_t* ctx, aml_pkg_length_t* out)
{
    const uint8_t* start = ctx->current;

    aml_pkg_lead_byte_t pkgLeadByte;
    if (aml_pkg_lead_byte_read(ctx, &pkgLeadByte) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLeadByte");
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
    for (uint32_t i = 0; i < pkgLeadByte.byteDataCount; i++)
    {
        uint8_t byte;
        if (aml_byte_data_read(ctx, &byte) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
            return ERR;
        }
        length |= ((uint32_t)byte) << (4 + i * 8);
    }

    // Output must not be greater than 2^28.
    if (length > (1ULL << 28))
    {
        AML_DEBUG_ERROR(ctx, "Package length out of range: %lu", length);
        errno = ERANGE;
        return ERR;
    }

    *out = length;
    return 0;
}

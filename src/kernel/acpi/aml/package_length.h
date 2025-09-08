#pragma once

#include <stdint.h>

#include "aml_state.h"

/**
 * @brief ACPI AML Package Length Encoding
 * @defgroup kernel_acpi_aml_package_length Package Length
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.4 of the ACPI specification.
 *
 * @{
 */

typedef uint64_t aml_pkg_length_t;

/**
 * @brief Parse a PkgLength structure.
 *
 * The PkgLength structure is defined as `PkgLeadByte | <pkgleadbyte bytedata> | <pkgleadbyte bytedata bytedata> |
 * <pkgleadbyte bytedata bytedata bytedata>`, where `PkgLeadByte` is defined as:
 * - bit 7-6: bytedata count that follows (0-3)
 * - bit 5-4: only used if pkglength <= 63
 * - bit 3-0: least significant package length nybble
 *
 * For more information, refer to the ACPI specification section 20.2.4.
 *
 * @param state The AML state.
 * @return On success, the package length. On failure, `ERR` and `errno` is set.
 */
static inline aml_pkg_length_t aml_pkg_length_parse(aml_state_t* state)
{
    uint64_t pkgLeadByte = aml_read_byte(state);
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
        uint64_t byte = aml_read_byte(state);
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

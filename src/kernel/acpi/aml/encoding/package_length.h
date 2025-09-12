#pragma once

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "data.h"

#include <errno.h>
#include <stdint.h>

/**
 * @brief ACPI AML Package Length Encoding
 * @defgroup kernel_acpi_aml_package_length Package Length
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.4 of the ACPI specification.
 *
 * @{
 */

/**
 * @brief ACPI AML PkgLength structure.
 */
typedef uint64_t aml_pkg_length_t;

/**
 * @brief ACPI AML PkgLeadByte structure.
 * @struct aml_pkg_lead_byte_t
 */
typedef struct
{
    uint8_t byteDataCount;          //!< Amount of ByteData structures that come after the lead byte.
    uint8_t smallLengthBits;        //!< Stores the total package length if the pkglength <= 63 which is equivalent to
                                    //!< byteDataCount == 0 else it must be 0.
    uint8_t leastSignificantNybble; //!< Least significant nybble of the package length.
} aml_pkg_lead_byte_t;

/**
 * @brief Reads a PkgLeadByte structure from the AML byte stream.
 *
 * The PkgLeadByte structure is defined as:
 * - bit 7-6: bytedata count that follows (0-3)
 * - bit 5-4: only used if pkglength <= 63
 * - bit 3-0: least significant package length nybble
 *
 * @param state The AML state.
 * @param out The output buffer to store the lead byte.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_pkg_lead_byte_read(aml_state_t* state, aml_pkg_lead_byte_t* out)
{
    uint8_t pkgLeadByte;
    if (aml_state_read(state, &pkgLeadByte, 1) != 1)
    {
        errno = ENODATA;
        return ERR;
    }

    out->byteDataCount = (pkgLeadByte >> 6) & 0x03;   // bits (7-6)
    out->smallLengthBits = pkgLeadByte & 0x3F;        // bits (5-0)
    out->leastSignificantNybble = pkgLeadByte & 0x0F; // bits (3-0)

    // If more bytes follow, then bits 4 and 5 must be zero.
    if (out->byteDataCount != 0 && ((pkgLeadByte >> 4) & 0x03) != 0)
    {
        AML_DEBUG_INVALID_STRUCTURE("PkgLeadByte");
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}
/**
 * @brief Reads a PkgLength structure from the AML byte stream.
 *
 * The PkgLength structure is defined as `PkgLength := PkgLeadByte | <pkgleadbyte bytedata> | <pkgleadbyte bytedata
 * bytedata> | <pkgleadbyte bytedata bytedata bytedata>`.
 *
 * @param state The AML state.
 * @param out The output buffer to store the package length.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
static inline uint64_t aml_pkg_length_read(aml_state_t* state, aml_pkg_length_t* out)
{
    aml_pkg_lead_byte_t pkgLeadByte;
    if (aml_pkg_lead_byte_read(state, &pkgLeadByte) == ERR)
    {
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
    for (uint8_t i = 0; i < pkgLeadByte.byteDataCount; i++)
    {
        aml_byte_data_t byteData;
        if (aml_byte_data_read(state, &byteData) == ERR)
        {
            return ERR;
        }
        length |= ((uint64_t)byteData) << (4 + i * 8);
    }

    // Output must not be greater than 2^28.
    if (length > (1ULL << 28))
    {
        AML_DEBUG_INVALID_STRUCTURE("PkgLength");
        errno = ERANGE;
        return ERR;
    }

    *out = length;
    return 0;
}

/** @} */

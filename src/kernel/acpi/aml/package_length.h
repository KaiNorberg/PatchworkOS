#pragma once

#include <stdint.h>

/**
 * @brief ACPI AML Package Length Encoding
 * @defgroup kernel_acpi_aml_package_length Package Length
 * @ingroup kernel_acpi_aml
 *
 * The ACPI AML State is used to keep track of the virtual machine's state during the parsing of AML bytecode and
 * provides wrappers to read data from the ACPI AML stream.
 *
 * @{
 */

typedef uint64_t aml_pkg_length_t;

/**
 * @brief Reads the next data as a package length object from the AML bytecode stream.
 *
 * See section 20.2.4 of the ACPI specification.
 *
 * @param state The AML state.
 * @param out Pointer to destination where the package length will be stored.
 * @return On success, the number of bytes read. On error, `ERR` and `errno` is set.
 */
/*static inline uint64_t aml_state_read_pkg_length(aml_state_t* state, aml_pkg_length_t* out)
{
    uint8_t pkgLeadByte;
    if (!aml_state_read_byte(state, &pkgLeadByte))
    {
        errno = ENODATA;
        return ERR;
    }
    uint64_t bytesRead = 1;

    uint8_t bytedataCount = (pkgLeadByte >> 6) & 0b11; // bits (7-6)

    // If no bytes follow, then the first 6 bits store the package length.
    if (bytedataCount == 0)
    {
        *out = pkgLeadByte & 0b111111;
        return 1;
    }

    // If more bytes follow, then bits 4 and 5 must be zero.
    if (pkgLeadByte & (1 << 4) || pkgLeadByte & (1 << 5))
    {
        errno = EILSEQ;
        return ERR;
    }

    // Bits 0 to 3 in pkgLeadByte becomes the least significant bits in the length, followed by the next bytes.
    *out = (pkgLeadByte & 0b1111);
    for (uint8_t i = 0; i < bytedataCount; i++)
    {
        uint8_t byte;
        if (!aml_state_read_byte(state, &byte))
        {
            errno = ENODATA;
            return ERR;
        }
        bytesRead += 1;
        *out |= ((uint64_t)byte) << (i * 8 + 4);
    }

    // Output must not be greater than 2^28.
    if (*out > (1ULL << 28))
    {
        errno = ERANGE;
        return ERR;
    }

    return bytesRead;
}*/

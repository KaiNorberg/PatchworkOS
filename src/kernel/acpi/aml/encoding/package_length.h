#pragma once

#include <stdint.h>

typedef struct aml_state aml_state_t;
typedef struct aml_scope aml_scope_t;

/**
 * @brief Package Length Encoding
 * @defgroup kernel_acpi_aml_encoding_package_length Package Length
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.4 of the ACPI specification.
 *
 * @{
 */

/**
 * @brief PkgLength structure.
 */
typedef uint64_t aml_pkg_length_t;

/**
 * @brief PkgLeadByte structure.
 * @struct aml_pkg_lead_byte_t
 */
typedef struct
{
    uint8_t byteDataCount;          ///< Amount of ByteData structures that come after the lead byte.
    uint8_t smallLengthBits;        ///< Stores the total package length if the pkglength <= 63 which is equivalent to
                                    ///< byteDataCount == 0 else it must be 0.
    uint8_t leastSignificantNybble; ///< Least significant nybble of the package length.
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
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_pkg_lead_byte_read(aml_state_t* state, aml_pkg_lead_byte_t* out);

/**
 * @brief Reads a PkgLength structure from the AML byte stream.
 *
 * The PkgLength structure is defined as `PkgLength := PkgLeadByte | <pkgleadbyte bytedata> | <pkgleadbyte bytedata
 * bytedata> | <pkgleadbyte bytedata bytedata bytedata>`.
 *
 * A PkgLength structure specifies the length from its own start to the end of the data for the operation/structure it
 * is part of, as such the PkgLength includes the length of the PkgLength structure itself.
 *
 * @see Section 5.4.1 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param out The output buffer to store the package length.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_pkg_length_read(aml_state_t* state, aml_pkg_length_t* out);

/** @} */

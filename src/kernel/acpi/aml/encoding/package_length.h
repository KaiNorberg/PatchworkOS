#pragma once

#include <stdint.h>

#include "acpi/aml/aml_state.h"

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
 * @brief Reads a PkgLength structure from the AML byte stream.
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
uint64_t aml_pkg_length_read(aml_state_t* state);

/** @} */

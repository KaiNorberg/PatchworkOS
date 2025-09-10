#pragma once

#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_op.h"

#include <stdint.h>

/**
 * @brief ACPI AML Named Objects Encoding
 * @defgroup kernel_acpi_aml_named Named Objects
 * @ingroup kernel_acpi_aml
 *
 * Not to be confused with "ACPI AML Name Objects Encoding".
 *
 * See section 20.2.5.2 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief ACPI AML Region Space Encoding
 * @enum aml_region_space_t
 */
typedef enum
{
    AML_REGION_SYSTEM_MEMORY = 0x00,
    AML_REGION_SYSTEM_IO = 0x01,
    AML_REGION_PCI_CONFIG = 0x02,
    AML_REGION_EMBEDDED_CONTROL = 0x03,
    AML_REGION_SM_BUS = 0x04,
    AML_REGION_SYSTEM_CMOS = 0x05,
    AML_REGION_PCI_BAR_TARGET = 0x06,
    AML_REGION_IPMI = 0x07,
    AML_REGION_GENERAL_PURPOSE_IO = 0x08,
    AML_REGION_GENERIC_SERIAL_BUS = 0x09,
    AML_REGION_PCC = 0x0A,
    AML_REGION_OEM_MIN = 0x80,
    AML_REGION_OEM_MAX = 0xFF,
} aml_region_space_t;

/**
 * @brief Reads a RegionSpace structure from the AML byte stream.
 *
 * A RegionSpace structure is defined as `ByteData`.
 *
 * @param state The AML state.
 * @param out The output buffer to store the region space.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
uint64_t aml_region_space_read(aml_state_t* state, aml_region_space_t* out);

/**
 * @brief ACPI AML RegionOffset structure.
 */
typedef uint64_t aml_region_offset_t;

/**
 * @brief Reads a RegionOffset structure from the AML byte stream.
 *
 * A RegionOffset structure is defined as `TermArg => Integer`.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @param out The output buffer to store the region offset.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
uint64_t aml_region_offset_read(aml_state_t* state, aml_scope_t* scope, aml_region_offset_t* out);

/**
 * @brief ACPI AML RegionLen structure.
 */
typedef uint64_t aml_region_len_t;

/**
 * @brief Reads a RegionLen structure from the AML byte stream.
 *
 * A RegionLen structure is defined as `TermArg => Integer`.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @param out The output buffer to store the region length.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
uint64_t aml_region_len_read(aml_state_t* state, aml_scope_t* scope, aml_region_len_t* out);

/**
 * @brief Reads a DefOpRegion structure from the AML byte stream.
 *
 * A DefOpRegion structure is defined as `OpRegionOp NameString RegionSpace RegionOffset RegionLen`.
 *
 * See section 19.6.100 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @param op The AML op, should have been read by the caller.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
uint64_t aml_def_op_region_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op);

 /**
  * @brief Reads a NamedObj structure from the AML byte stream.
  *
  * A NamedObj structure is defined as `DefBankField | DefCreateBitField | DefCreateByteField | DefCreateDWordField | DefCreateField | DefCreateQWordField | DefCreateWordField | DefDataRegion | DefExternal | DefOpRegion | DefPowerRes | DefThermalZone`.
  *
  * @param state The AML state.
  * @param scope The AML scope.
  * @param op The AML op, should have been read by the caller.
  * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
  */
uint64_t aml_named_obj_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op);

/** @} */

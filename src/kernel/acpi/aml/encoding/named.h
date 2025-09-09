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
 * @brief Reads a DefOpRegion structure from the AML byte stream.
 *
 * A DefOpRegion structure is defined as `OpRegionOp NameString RegionSpace RegionOffset RegionLen`. Note that `OpRegionOp` should have already
 * been read and passed by the caller in `op`.
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

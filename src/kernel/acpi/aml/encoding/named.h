#pragma once

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "data.h"
#include "name.h"
#include "term.h"

#include "log/log.h"

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

#include "named_region_space.h"

/**
 * @brief Reads a RegionSpace structure from the AML byte stream.
 *
 * A RegionSpace structure is defined as `ByteData`.
 *
 * @param state The AML state.
 * @param out The output buffer to store the region space.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_region_space_read(aml_state_t* state, aml_region_space_t* out)
{
    aml_byte_data_t byteData;
    if (aml_byte_data_read(state, &byteData) == ERR)
    {
        return ERR;
    }

    if (byteData > AML_REGION_PCC && byteData < AML_REGION_OEM_MIN)
    {
        AML_DEBUG_INVALID_STRUCTURE("ByteData");
        errno = EILSEQ;
        return ERR;
    }

    *out = byteData;
    return 0;
}

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
static inline uint64_t aml_region_offset_read(aml_state_t* state, aml_scope_t* scope, aml_region_offset_t* out)
{
    return aml_termarg_read_integer(state, scope, out);
}

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
static inline uint64_t aml_region_len_read(aml_state_t* state, aml_scope_t* scope, aml_region_len_t* out)
{
    return aml_termarg_read_integer(state, scope, out);
}

/**
 * @brief Reads a DefOpRegion structure from the AML byte stream.
 *
 * A DefOpRegion structure is defined as `OpRegionOp NameString RegionSpace RegionOffset RegionLen`.
 *
 * See section 19.6.100 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_def_op_region_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t opRegionOp;
    if (aml_value_read(state, &opRegionOp) == ERR)
    {
        return ERR;
    }

    if (opRegionOp.num != AML_OPREGION_OP)
    {
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        return ERR;
    }

    aml_region_space_t regionSpace;
    if (aml_region_space_read(state, &regionSpace) == ERR)
    {
        return ERR;
    }

    aml_region_offset_t regionOffset;
    if (aml_region_offset_read(state, scope, &regionOffset) == ERR)
    {
        return ERR;
    }

    aml_region_len_t regionLen;
    if (aml_region_len_read(state, scope, &regionLen) == ERR)
    {
        return ERR;
    }

    aml_node_t* node = aml_add_node_at_name_string(&nameString, scope->location, AML_NODE_OPREGION);
    if (node == NULL)
    {
        return ERR;
    }
    node->opregion.space = regionSpace;
    node->opregion.offset = regionOffset;
    node->opregion.len = regionLen;

    AML_DEBUG_UNIMPLEMENTED_VALUE(&opRegionOp);
    errno = ENOSYS;
    return ERR;
}

/**
 * @brief Reads a NamedObj structure from the AML byte stream.
 *
 * A NamedObj structure is defined as `DefBankField | DefCreateBitField | DefCreateByteField | DefCreateDWordField |
 * DefCreateField | DefCreateQWordField | DefCreateWordField | DefDataRegion | DefExternal | DefOpRegion | DefPowerRes |
 * DefThermalZone`.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_named_obj_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.num)
    {
    case AML_OPREGION_OP:
        return aml_def_op_region_read(state, scope);
    default:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    }
}

/** @} */

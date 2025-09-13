#pragma once

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "data.h"
#include "name.h"
#include "package_length.h"
#include "term.h"

/**
 * @brief ACPI AML Named Objects Encoding
 * @defgroup kernel_acpi_aml_named Named Objects
 * @ingroup kernel_acpi_aml
 *
 * Not to be confused with "ACPI AML Name Objects Encoding".
 *
 * See section 20.2.5.2 of the ACPI specification for more details.
 *
 * Note that version 6.6 of the ACPI specification has a few minor mistakes in the definition of the NamedObj structure,
 * its supposed several stuctures that it does not. If you check version 4.0 of the specification section 19.2.5.2 you
 * can confirm that that these structures are supposed to be there but have, somehow, been forgotten. These structures
 * include DefField and DefMethod (more may be noticed in the future), we add all missing structures to our definition.
 *
 * @{
 */

#include "named_types.h"

/**
 * @brief Reads a RegionSpace structure from the AML byte stream.
 *
 * A RegionSpace structure is defined as `RegionSpace := ByteData`.
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
 * @brief Reads a RegionOffset structure from the AML byte stream.
 *
 * A RegionOffset structure is defined as `RegionOffset := TermArg => Integer`.
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
 * @brief Reads a RegionLen structure from the AML byte stream.
 *
 * A RegionLen structure is defined as `RegionLen := TermArg => Integer`.
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
 * A DefOpRegion structure is defined as `DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen`.
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
    node->opregion.length = regionLen;

    return 0;
}

/**
 * @brief Reads a FieldFlags structure from the AML byte stream.
 *
 * Clean up definition.
 *
 * A FieldFlags structure is defined as `FieldFlags := ByteData`, where
 * - bit 0-3: AccessType
 *   - 0 AnyAcc
 *   - 1 ByteAcc
 *   - 2 WordAcc
 *   - 3 DWordAcc
 *   - 4 QWordAcc
 *   - 5 BufferAcc
 *   - 6 Reserved
 *
 * - bits 7-15 Reserved
 *
 * - bit 4: LockRule
 *   - 0 NoLock
 *   - 1 Lock
 *
 * - bit 5-6: UpdateRule
 *   - 0 Preserve
 *   - 1 WriteAsOnes
 *   - 2 WriteAsZeros
 *
 * - bit 7: Reserved (must be 0)
 *
 * @param state The AML state.
 * @param out The buffer to store the FieldFlags structure.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_field_flags_read(aml_state_t* state, aml_field_flags_t* out)
{
    aml_byte_data_t flags;
    if (aml_byte_data_read(state, &flags) == ERR)
    {
        return ERR;
    }

    if (flags & 0xC0)
    {
        errno = EILSEQ;
        return ERR;
    }

    aml_access_type_t accessType = flags & 0xF;
    if (accessType > AML_ACCESS_TYPE_BUFFER)
    {
        errno = EINVAL;
        return ERR;
    }

    *out = (aml_field_flags_t){
        .accessType = accessType,
        .lockRule = (flags >> 4) & 0x1,
        .updateRule = (flags >> 5) & 0x3,
    };

    return 0;
}

/**
 * @brief Reads a NamedField structure from the AML byte stream.
 *
 * A NamedField structure is defined as `NamedField := NameSeg PkgLength`
 *
 * See section 19.6.48 of the ACPI specification for more details about the Field Operation.
 *
 * @param state The AML state.
 * @param ctx The AML field list context.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_named_field_read(aml_state_t* state, aml_field_list_ctx_t* ctx)
{
    aml_name_seg_t name;
    if (aml_name_seg_read(state, &name) == ERR)
    {
        return ERR;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    aml_node_t* newNode = aml_add_node(ctx->opregion, name.name, AML_NODE_FIELD);
    if (!newNode)
    {
        return ERR;
    }
    newNode->field.flags = ctx->flags;
    newNode->field.offset = ctx->currentOffset;
    newNode->field.size = pkgLength;

    return 0;
}

/**
 * @brief Reads a FieldElement structure from the AML byte stream.
 *
 * The FieldElement structure is defined as `FieldElement := NamedField | ReservedField | AccessField |
 * ExtendedAccessField | ConnectField`.
 *
 * See section 19.6.48 of the ACPI specification for more details about the Field Operation.
 *
 * @param state The AML state.
 * @param ctx The AML field list context.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static uint64_t aml_field_element_read(aml_state_t* state, aml_field_list_ctx_t* ctx)
{
    aml_value_t value;
    if (aml_value_peek_no_ext(state, &value) == ERR)
    {
        return ERR;
    }

    // Is NameField
    if (AML_IS_LEAD_NAME_CHAR(&value))
    {
        return aml_named_field_read(state, ctx);
    }

    AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
    errno = ENOSYS;
    return ERR;
}

/**
 * @brief Reads a FieldList structure from the AML byte stream.
 *
 * The FieldList structure is defined as `FieldList := Nothing | <fieldelement fieldlist>`.
 *
 * See section 19.6.48 of the ACPI specification for more details about the Field Operation.
 *
 * @param state The AML state.
 * @param ctx The AML field list context.
 * @param end The index at which the FieldList ends.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static uint64_t aml_field_list_read(aml_state_t* state, aml_field_list_ctx_t* ctx, aml_address_t end)
{
    while (end > state->instructionPointer)
    {
        // End of buffer not reached => byte is not nothing => must be a FieldElement.
        if (aml_field_element_read(state, ctx) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

/**
 * @brief Reads a DefField structure from the AML byte stream.
 *
 * The DefField structure is defined as `DefField := FieldOp PkgLength NameString FieldFlags FieldList`.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_def_field_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_address_t start = state->instructionPointer;

    aml_value_t fieldOp;
    if (aml_value_read(state, &fieldOp) == ERR)
    {
        return ERR;
    }

    if (fieldOp.num != AML_FIELD_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&fieldOp);
        errno = EILSEQ;
        return ERR;
    }

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        return ERR;
    }

    aml_field_flags_t fieldFlags;
    if (aml_field_flags_read(state, &fieldFlags) == ERR)
    {
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_field_list_ctx_t ctx = {
        .opregion = aml_name_string_walk(&nameString, scope->location),
        .flags = fieldFlags,
        .currentOffset = 0,
    };

    if (aml_field_list_read(state, &ctx, end) == ERR)
    {
        return ERR;
    }

    return 0;
}

/**
 * @brief Reads a DefMethod structure from the AML byte stream.
 *
 * The DefField structure is defined as `DefMethod := MethodOp PkgLength NameString MethodFlags TermList`.
 *
 * @param state The AML state.
 * @param scope The AML scope.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_def_method_read(aml_state_t* state, aml_scope_t* scope)
{
    AML_DEBUG_UNIMPLEMENTED_STRUCTURE("DefMethod");
    errno = ENOSYS;
    return ERR;
}

/**
 * @brief Reads a NamedObj structure from the AML byte stream.
 *
 * The NamedObj structure is defined as `NamedObj := DefBankField | DefCreateBitField | DefCreateByteField |
 * DefCreateDWordField | DefCreateField | DefCreateQWordField | DefCreateWordField | DefDataRegion | DefExternal |
 * DefOpRegion | DefPowerRes | DefThermalZone | DefField | DefMethod`.
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
    case AML_FIELD_OP:
        return aml_def_field_read(state, scope);
    case AML_METHOD_OP:
        return aml_def_method_read(state, scope);
    default:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    }
}

/** @} */

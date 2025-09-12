#pragma once

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "data.h"
#include "name.h"
#include "term.h"
#include "package_length.h"

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
 * @brief ACPI AML RegionOffset structure.
 */
typedef uint64_t aml_region_offset_t;

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
 * @brief ACPI AML RegionLen structure.
 */
typedef uint64_t aml_region_len_t;

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
    node->opregion.len = regionLen;

    return 0;
}

/**
 * @brief Enum for all field access types, bits 0-3 of FieldFlags.
 */
typedef enum
{
    AML_FIELD_ACCESS_ANY = 0,
    AML_FIELD_ACCESS_BYTE = 1,
    AML_FIELD_ACCESS_WORD = 2,
    AML_FIELD_ACCESS_DWORD = 3,
    AML_FIELD_ACCESS_QWORD = 4,
    AML_FIELD_ACCESS_BUFFER = 5,
} aml_field_access_type_t;

/**
 * @brief Enum for all field lock rules, bit 4 of FieldFlags.
 */
typedef enum
{
    AML_FIELD_LOCK_NO_LOCK = 0,
    AML_FIELD_LOCK_LOCK = 1,
} aml_field_lock_rule_t;

/**
 * @brief Enum for all field update rules, bits 5-6 of FieldFlags.
 */
typedef enum
{
    AML_FIELD_UPDATE_PRESERVE = 0,
    AML_FIELD_UPDATE_WRITE_AS_ONES = 1,
    AML_FIELD_UPDATE_WRITE_AS_ZEROS = 2,
} aml_field_update_rule_t;

/**
 * @brief ACPI AML FieldFlags structure.
 */
typedef struct
{
    aml_field_access_type_t accessType;
    aml_field_lock_rule_t lockRule;
    aml_field_update_rule_t updateRule;
} aml_field_flags_t;

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

    aml_field_access_type_t accessType = flags & 0xF;
    if (accessType > AML_FIELD_ACCESS_BUFFER)
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

    /*aml_field_list_t fieldList;
    if (aml_field_list_read(state, &fieldList) == ERR)
    {
        return ERR;
    }*/

    AML_DEBUG_UNIMPLEMENTED_STRUCTURE("FieldList");
    errno = ENOSYS;
    return ERR;
}

/**
 * @brief Reads a NamedObj structure from the AML byte stream.
 *
 * Version 6.6 of the ACPI specification has a minor mistake in the definition of the NamedObj structure, its supposed
 * to contain `DefField` but does not, instead `DefField` is left without any parent definition. If you check
 * version 4.0 section 19.2.5.2 you can confirm that `DefField` is supposed to be part of the NamedObj structure so we
 * add it for our definition.
 *
 * The NamedObj structure is defined as `NamedObj := DefBankField | DefCreateBitField | DefCreateByteField |
 * DefCreateDWordField | DefCreateField | DefCreateQWordField | DefCreateWordField | DefDataRegion | DefExternal |
 * DefOpRegion | DefPowerRes | DefThermalZone | DefField`.
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
    default:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    }
}

/** @} */

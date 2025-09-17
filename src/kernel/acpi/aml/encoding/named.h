#pragma once

#include "acpi/aml/aml_state.h"
#include "data_integers.h"

typedef struct aml_node aml_node_t;

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
 * @brief ACPI AML RegionOffset structure.
 */
typedef uint64_t aml_region_offset_t;

/**
 * @brief ACPI AML RegionLen structure.
 */
typedef uint64_t aml_region_len_t;

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
 * @brief Enum for all field access types, bits 0-3 of FieldFlags.
 * @enum aml_access_type_t
 */
typedef enum
{
    AML_ACCESS_TYPE_ANY = 0,
    AML_ACCESS_TYPE_BYTE = 1,
    AML_ACCESS_TYPE_WORD = 2,
    AML_ACCESS_TYPE_DWORD = 3,
    AML_ACCESS_TYPE_QWORD = 4,
    AML_ACCESS_TYPE_BUFFER = 5,
} aml_access_type_t;

/**
 * @brief Enum for all field lock rules, bit 4 of FieldFlags.
 * @enum aml_lock_rule_t
 */
typedef enum
{
    AML_LOCK_RULE_NO_LOCK = 0,
    AML_LOCK_RULE_LOCK = 1,
} aml_lock_rule_t;

/**
 * @brief Enum for all field update rules, bits 5-6 of FieldFlags.
 * @enum aml_update_rule_t
 */
typedef enum
{
    AML_UPDATE_RULE_PRESERVE = 0,
    AML_UPDATE_RULE_WRITE_AS_ONES = 1,
    AML_UPDATE_RULE_WRITE_AS_ZEROS = 2,
} aml_update_rule_t;

/**
 * @brief ACPI AML FieldFlags structure.
 * @struct aml_field_flags_t
 */
typedef struct
{
    aml_access_type_t accessType;
    aml_lock_rule_t lockRule;
    aml_update_rule_t updateRule;
} aml_field_flags_t;

/**
 * @brief Enum for all FieldList types.
 * @enum aml_field_list_type_t
 */
typedef enum
{
    AML_FIELD_LIST_TYPE_NORMAL, //!< FieldList is part of a DefField.
    AML_FIELD_LIST_TYPE_BANK,   //!< FieldList is part of a BankField.
    AML_FIELD_LIST_TYPE_INDEX,  //!< FieldList is part of an IndexField.
} aml_field_list_type_t;

/**
 * @brief Represents a size in bits within an opregion.
 */
typedef uint64_t aml_bit_size_t;

/**
 * @brief Context passed to lower functions by `aml_field_list_read()`.
 * @struct aml_field_list_ctx_t
 */
typedef struct
{
    aml_field_list_type_t type;   //!< The type of FieldList.
    aml_field_flags_t flags;      //!< The flags of the FieldList.
    aml_bit_size_t currentOffset; //!< The current offset within the opregion.
    union {
        struct
        {
            aml_node_t* opregion;
        } normal;
        struct
        {
            aml_node_t* indexNode;
            aml_node_t* dataNode;
        } index;
    };
} aml_field_list_ctx_t;

/**
 * @brief ACPI AML SyncLevel Encoding
 * @enum aml_sync_level_t
 */
typedef uint8_t aml_sync_level_t;

/**
 * @brief ACPI AML MethodFlags structure.
 * @struct aml_method_flags_t
 */
typedef struct
{
    uint8_t argCount;           //!< Amount of arguments (0-7)
    bool isSerialized;          //!< true if method is serialized, false if not
    aml_sync_level_t syncLevel; //!< Synchronization level (0-15)
} aml_method_flags_t;

/**
 * @brief ACPI AML ProcID structure, deprecated in version 6.4 of the ACPI specification.
 */
typedef aml_byte_data_t aml_proc_id_t;

/**
 * @brief ACPI AML PblkAddr structure, deprecated in version 6.4 of the ACPI specification.
 */
typedef aml_dword_data_t aml_pblk_addr_t;

/**
 * @brief ACPI AML PblkLen structure, deprecated in version 6.4 of the ACPI specification.
 */
typedef aml_byte_data_t aml_pblk_len_t;

/**
 * @brief Reads a RegionSpace structure from the AML byte stream.
 *
 * A RegionSpace structure is defined as `RegionSpace := ByteData`.
 *
 * @param state The AML state.
 * @param out The output buffer to store the region space.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_region_space_read(aml_state_t* state, aml_region_space_t* out);

/**
 * @brief Reads a RegionOffset structure from the AML byte stream.
 *
 * A RegionOffset structure is defined as `RegionOffset := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out The output buffer to store the region offset.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_region_offset_read(aml_state_t* state, aml_node_t* node, aml_region_offset_t* out);

/**
 * @brief Reads a RegionLen structure from the AML byte stream.
 *
 * A RegionLen structure is defined as `RegionLen := TermArg => Integer`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out The output buffer to store the region length.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_region_len_read(aml_state_t* state, aml_node_t* node, aml_region_len_t* out);

/**
 * @brief Reads a DefOpRegion structure from the AML byte stream.
 *
 * A DefOpRegion structure is defined as `DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen`.
 *
 * See section 19.6.100 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_op_region_read(aml_state_t* state, aml_node_t* node);

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
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_flags_read(aml_state_t* state, aml_field_flags_t* out);

/**
 * @brief Reads a NamedField structure from the AML byte stream.
 *
 * A NamedField structure is defined as `NamedField := NameSeg PkgLength`
 *
 * See section 19.6.48 of the ACPI specification for more details about the Field Operation.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param ctx The AML field list context.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_named_field_read(aml_state_t* state, aml_node_t* node, aml_field_list_ctx_t* ctx);

/**
 * @brief Reads a ReservedField structure from the AML byte stream.
 *
 * A ReservedField structure is defined as `ReservedField := 0x00 PkgLength`.
 *
 * See section 19.6.48 of the ACPI specification for more details about the Field Operation.
 *
 * @param state The AML state.
 * @param ctx The AML field list context.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_reserved_field_read(aml_state_t* state, aml_field_list_ctx_t* ctx);

/**
 * @brief Reads a FieldElement structure from the AML byte stream.
 *
 * The FieldElement structure is defined as `FieldElement := NamedField | ReservedField | AccessField |
 * ExtendedAccessField | ConnectField`.
 *
 * See section 19.6.48 of the ACPI specification for more details about the Field Operation.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param ctx The AML field list context.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_element_read(aml_state_t* state, aml_node_t* node, aml_field_list_ctx_t* ctx);

/**
 * @brief Reads a FieldList structure from the AML byte stream.
 *
 * The FieldList structure is defined as `FieldList := Nothing | <fieldelement fieldlist>`.
 *
 * See section 19.6.48 of the ACPI specification for more details about the Field Operation.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param ctx The AML field list context.
 * @param end The index at which the FieldList ends.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_field_list_read(aml_state_t* state, aml_node_t* node, aml_field_list_ctx_t* ctx, aml_address_t end);

/**
 * @brief Reads a DefField structure from the AML byte stream.
 *
 * The DefField structure is defined as `DefField := FieldOp PkgLength NameString FieldFlags FieldList`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_field_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a DefIndexField structure from the AML byte stream.
 *
 * The DefIndexField structure is defined as `DefIndexField := IndexFieldOp PkgLength NameString NameString FieldFlags
 * FieldList`.
 *
 * See section 19.6.64 of the ACPI specification for more details.
 *
 * IndexFields can be a bit confusing, but the basic idea is that you have two fields, one for the index and one for the
 * data. The index field in this case can be thought of as a "selector", and the data field is where we find the actual
 * data we "selected". For example, to perform a read, we first write an index to the index field, and then read the
 * data from the data field. The index is typically an offset into an array or table, and the data is the value at that
 * offset.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_index_field_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a MethodFlags structure from the AML byte stream.
 *
 * A MethodFlags structure is defined as `MethodFlags := ByteData`, where
 * - bit 0-2: ArgCount (0-7)
 * - bit 3:
 *  - 0: NotSerialized
 *  - 1: Serialized
 * - bit 4-7: SyncLevel (0x00-0x0F)
 *
 * @param state The AML state.
 * @param out The output buffer to store the MethodFlags structure.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_flags_read(aml_state_t* state, aml_method_flags_t* out);

/**
 * @brief Reads a DefMethod structure from the AML byte stream.
 *
 * The DefField structure is defined as `DefMethod := MethodOp PkgLength NameString MethodFlags TermList`.
 *
 * See section 19.6.85 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_method_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a DefDevice structure from the AML byte stream.
 *
 * The DefDevice structure is defined as `DefDevice := DeviceOp PkgLength NameString TermList`.
 *
 * See section 19.6.31 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_device_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a SyncFlags structure from the AML byte stream.
 *
 * A SyncFlags structure is defined as `SyncFlags := ByteData`, where
 * - bit 0-3: SyncLevel (0x00-0x0F)
 * - bit 4-7: Reserved (must be 0)
 *
 * @param state The AML state.
 * @param out The output buffer to store the SyncFlags structure.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_sync_flags_read(aml_state_t* state, aml_sync_level_t* out);

/**
 * @brief Reads a DefMutex structure from the AML byte stream.
 *
 * The DefMutex structure is defined as `DefMutex := MutexOp NameString SyncFlags`.
 *
 * See section 19.6.89 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_mutex_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a ProcID structure from the AML byte stream. Deprecated in ACPI 6.4 but still supported.
 *
 * A ProcID structure is defined as `ProcID := ByteData`.
 *
 * @param state The AML state.
 * @param out The output buffer to store the processor ID.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_proc_id_read(aml_state_t* state, aml_proc_id_t* out);

/**
 * @brief Reads a PblkAddr structure from the AML byte stream. Deprecated in ACPI 6.4 but still supported.
 *
 * A PblkAddr structure is defined as `PblkAddr := DWordData`.
 *
 * @param state The AML state.
 * @param out The output buffer to store the Pblk address.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_pblk_addr_read(aml_state_t* state, aml_pblk_addr_t* out);

/**
 * @brief Reads a PblkLen structure from the AML byte stream. Deprecated in ACPI 6.4 but still supported.
 *
 * A PblkLen structure is defined as `PblkLen := ByteData`.
 *
 * @param state The AML state.
 * @param out The output buffer to store the Pblk length.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_pblk_len_read(aml_state_t* state, aml_pblk_len_t* out);

/**
 * @brief Reads a DefProcessor structure from the AML byte stream. Deprecated in ACPI 6.4 but still supported.
 *
 * The DefProcessor structure is defined as `DefProcessor := ProcessorOp PkgLength NameString ProcID PblkAddr PblkLen
 * TermList`.
 *
 * See section 20.2.7 of version 6.3 Errata A of the ACPI specification for more details on the grammar and
 * section 19.6.108 of the same for more details about its behavior.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_processor_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a NamedObj structure from the AML byte stream.
 *
 * Version 6.6 of the ACPI specification has a few minor mistakes in the definition of the NamedObj structure,
 * its supposed to contain several stuctures that it does not. If you check version 4.0 of the specification
 * section 19.2.5.2 you can confirm that that these structures are supposed to be there but have, somehow, been
 * forgotten. These structures include DefField, DefMethod, DefMutex, DefIndexField and DefDevice (more may be noticed
 * in the future), we add all missing structures to our definition of NamedObj. I have no idea how there are this many
 * mistakes in the latest version of the ACPI specification.
 *
 * The DefProcessor structure was deprecated in version 6.4 of the ACPI specification, but we still support it.
 *
 * The NamedObj structure is defined as `NamedObj := DefBankField | DefCreateBitField | DefCreateByteField |
 * DefCreateDWordField | DefCreateField | DefCreateQWordField | DefCreateWordField | DefDataRegion | DefExternal |
 * DefOpRegion | DefPowerRes | DefThermalZone | DefField | DefMethod | DefDevice | DefMutex | DefProcessor |
 * DefIndexField`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_named_obj_read(aml_state_t* state, aml_node_t* node);

/** @} */

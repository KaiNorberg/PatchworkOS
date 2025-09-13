#pragma once

#include "acpi/aml/aml_state.h"

#include <stdint.h>

typedef struct aml_node aml_node_t;

/**
 * @ingroup kernel_acpi_aml_named
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
 */
typedef enum
{
    AML_LOCK_RULE_NO_LOCK = 0,
    AML_LOCK_RULE_LOCK = 1,
} aml_lock_rule_t;

/**
 * @brief Enum for all field update rules, bits 5-6 of FieldFlags.
 */
typedef enum
{
    AML_UPDATE_RULE_PRESERVE = 0,
    AML_UPDATE_RULE_WRITE_AS_ONES = 1,
    AML_UPDATE_RULE_WRITE_AS_ZEROS = 2,
} aml_update_rule_t;

/**
 * @brief ACPI AML FieldFlags structure.
 */
typedef struct
{
    aml_access_type_t accessType;
    aml_lock_rule_t lockRule;
    aml_update_rule_t updateRule;
} aml_field_flags_t;

/**
 * @brief Context passed to lower functions by `aml_field_list_read()`.
 */
typedef struct
{
    aml_node_t* opregion;        //!< The opregion the FieldList is part of, determined by the NameString.
    aml_field_flags_t flags;     //!< The flags of the FieldList.
    aml_address_t currentOffset; //!< The current offset within the opregion.
} aml_field_list_ctx_t;

/** @} */

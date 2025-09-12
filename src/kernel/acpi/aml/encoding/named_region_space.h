#pragma once

/**
 * @ingroup kernel_acpi_aml_named
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

/** @} */

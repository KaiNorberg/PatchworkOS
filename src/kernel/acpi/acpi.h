#pragma once

#include <boot/boot_info.h>
#include <common/defs.h>

#include <stdint.h>

/**
 * @brief Advanced Configuration and Power Interface
 * @defgroup kernel_acpi acpi
 * @ingroup kernel
 *
 * We use version 6.6 of the ACPI specification, but it contains minor mistakes or deprecated features that we use other
 * versions to straighten out. If the "ACPI specification" is ever sourced, without mentioning its version, assume
 * version 6.6.
 *
 * Take a look at this [osdev post](https://f.osdev.org/viewtopic.php?t=29070) if you want to understand how annoying
 * the ACPI spec is.
 *
 * Primary Source: [ACPI Specification Version 6.6](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 *
 * Other Sources:
 * - [ACPI Specification Version 6.3](https://uefi.org/sites/default/files/resources/ACPI_Spec_6_3_A_Oct_6_2020.pdf)
 * - [ACPI Specification Version 4.0](https://uefi.org/sites/default/files/resources/ACPI_4.pdf)
 * - [lai library](https://github.com/managarm/lai)
 *
 * @{
 */

#define ACPI_REVISION_1_0 0
#define ACPI_REVISION_2_0 2

typedef struct PACKED
{
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checkSum;
    uint8_t oemId[6];
    uint8_t oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorID;
    uint32_t creatorRevision;
} acpi_header_t;

typedef struct PACKED
{
    char signature[8];
    uint8_t checksum;
    char oemId[6];
    uint8_t revision;
    uint32_t acpiAddress;
    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t extendedChecksum;
    uint8_t reserved[3];
} xsdp_t;

typedef struct PACKED
{
    acpi_header_t header;
    acpi_header_t* tables[];
} xsdt_t;

/**
 * @brief Initialize the entire ACPI subsystem
 *
 * Will also initalize all ACPI subsystems, for example namespaces and tables.
 *
 * @param xsdp Pointer to the XSDP structure provided by the bootloader.
 * @param map Pointer to the memory map provided by the bootloader.
 */
void acpi_init(xsdp_t* xsdp, boot_memory_map_t* map);

bool acpi_is_checksum_valid(void* table, uint64_t length);

/** @} */

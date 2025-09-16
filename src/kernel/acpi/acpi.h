#pragma once

#include "fs/sysfs.h"

#include <boot/boot_info.h>
#include <common/defs.h>

#include <stdint.h>

/**
 * @brief Advanced Configuration and Power Interface
 * @defgroup kernel_acpi ACPI
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

/**
 * @brief The expected value of the revision field in the RSDP structure.
 *
 * See section 5.2.5.3 of the ACPI specification for more details.
 */
#define RSDP_CURRENT_REVISION 2

/**
 * @brief System Description Table Header
 * @struct sdt_header_t
 *
 * See section 5.2.6 of the ACPI specification for more details.
 */
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
} sdt_header_t;

/**
 * @brief Root System Description Pointer
 * @struct rsdp_t
 *
 * See section 5.2.5.3 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    char signature[8];
    uint8_t checksum;
    char oemId[6];
    uint8_t revision;
    uint32_t rsdtAddress;
    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t extendedChecksum;
    uint8_t reserved[3];
} rsdp_t;

/**
 * @brief Extended System Description Table
 * @struct xsdt_t
 *
 * See section 5.2.8 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    sdt_header_t header;
    sdt_header_t* tables[];
} xsdt_t;

/**
 * @brief Initialize the entire ACPI subsystem
 *
 * Will also initalize all ACPI subsystems, for example namespaces and tables.
 *
 * @param rsdp Pointer to the RSDP structure provided by the bootloader.
 * @param map Pointer to the memory map provided by the bootloader.
 */
void acpi_init(rsdp_t* rsdp, boot_memory_map_t* map);

/**
 * @brief Check if the sum of all bytes in a table is 0
 *
 * Used to validate the checksum of ACPI tables.
 *
 * @param table Pointer to the table structure.
 * @return true if the table is valid, false otherwise.
 */
bool acpi_is_checksum_valid(void* table, uint64_t length);

/**
 * @brief Retrieve the sysfs root directory for ACPI
 *
 * @return sysfs_dir_t* Pointer to the ACPI sysfs root directory.
 */
sysfs_dir_t* acpi_get_sysfs_root(void);

/** @} */

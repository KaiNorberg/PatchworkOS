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
 * Checklist for ACPI support from section 1.7.2 of the ACPI specification:
 * - [x] Use System Address Map Interfaces (this is done by the bootloader).
 * - [x] Find and consume the ACPI System Description Tables (this is done in `acpi_tables_init()`)
 * - [x] Interpret ACPI machine language (AML). (this is done in `aml_init()`)
 * - [X] Enumerate and configure motherboard devices described in the ACPI Namespace. (this is done in `acpi_devices_init()`)
 * - [ ] Interface with the power management timer. <-- We are here.
 * - [ ] Interface with the real-time clock wake alarm.
 * - [ ] Enter ACPI mode (on legacy hardware systems).
 * - [ ] Implement device power management policy.
 * - [ ] Implement power resource management.
 * - [ ] Implement processor power states in the scheduler idle handlers.
 * - [ ] Control processor and device performance states.
 * - [ ] Implement the ACPI thermal model.
 * - [ ] Support the ACPI Event programming model including handling SCI interrupts, managing fixed events, general-
 purpose events, embedded controller interrupts, and dynamic device support.
 * - [ ] Support acquisition and release of the Global Lock.
 * - [ ] Use the reset register to reset the system.
 * - [ ] Provide APIs to influence power management policy.
 * - [ ] Implement driver support for ACPI-defined devices.
 * - [ ] Implement APIs supporting the system indicators.
 * - [ ] Support all system states S1-S5.
 *
 * @see [Easier to read version of the ACPI Specification](https://uefi.org/specs/ACPI/6.6/index.html)
 * @see [ACPI Specification Version 6.6](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 * @see [ACPI Specification Version 6.3](https://uefi.org/sites/default/files/resources/ACPI_Spec_6_3_A_Oct_6_2020.pdf)
 * @see [ACPI Specification Version 4.0](https://uefi.org/sites/default/files/resources/ACPI_4.pdf)
 * @see [LAI Library](https://github.com/managarm/lai)
 *
 * @{
 */

/**
 * @brief The expected value of the revision field in the RSDP structure.
 *
 * @see Section 5.2.5.3 of the ACPI specification for more details.
 */
#define RSDP_CURRENT_REVISION 2

/**
 * @brief The length of the signature field in the SDT header structure.
 */
#define SDT_SIGNATURE_LENGTH 4

/**
 * @brief System Description Table Header
 * @struct sdt_header_t
 *
 * @see Section 5.2.6 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    uint8_t signature[SDT_SIGNATURE_LENGTH];
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
 * @see Section 5.2.5.3 of the ACPI specification for more details.
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
 * @see Section 5.2.8 of the ACPI specification for more details.
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

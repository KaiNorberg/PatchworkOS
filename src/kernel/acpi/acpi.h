#pragma once

#include <boot/boot_info.h>
#include <common/defs.h>

#include <stdint.h>

/**
 * @brief Advanced Configuration and Power Interface
 * @defgroup kernel_acpi acpi
 * @ingroup kernel
 * @{
 */

#define ACPI_REVISION_1_0 0
#define ACPI_REVISION_2_0 2

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

void acpi_init(xsdp_t* xsdp, boot_memory_map_t* map);

bool acpi_is_checksum_valid(void* table, uint64_t length);

/** @} */

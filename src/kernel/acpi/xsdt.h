#pragma once

#include "acpi.h"

#include <boot/boot_info.h>
#include <common/defs.h>

#include <stdint.h>



/**
 * @brief eXtended System Descriptor Table
 * @defgroup kernel_acpi_xsdt XSDT
 * @ingroup kernel_acpi
 * @{
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
} sdt_t;

typedef struct PACKED
{
    sdt_t header;
    sdt_t* tables[];
} xsdt_t;

/**
 * @brief Load the tables from the XSDT.
 *
 * Will be called by `acpi_init()`, should only be called once.
 *
 * @param xsdt The XSDT to load the tables from.
 * @return On success, the number of tables loaded. On failure, ERR.
 */
uint64_t xsdt_load_tables(xsdt_t* xsdt);

/**
 * @brief Lookup the n'th table matching the signature.
 *
 * @param signature The signature of the table to look up.
 * @param n The index of the table to look up (0 indexed).
 * @return The table if found, NULL otherwise.
 */
sdt_t* xsdt_lookup(const char* signature, uint64_t n);

/** @} */

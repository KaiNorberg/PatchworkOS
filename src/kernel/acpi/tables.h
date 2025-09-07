#pragma once

#include "acpi.h"

/**
 * @brief ACPI Tables
 * @defgroup kernel_acpi_tables ACPI Tables
 * @ingroup kernel_acpi
 * @{
 */

/**
 * @brief The maximum number of ACPI tables that are supported.
 *
 * As far as I know there should never be even close to this many on any machine.
 */
#define ACPI_MAX_TABLES 64

/**
 * @brief Enum for the `fadt_t::bootArchFlags` field.
 */
typedef enum
{
    FADT_BOOT_ARCH_PS2_EXISTS = (1 << 1)
} fadt_boot_arch_flags_t;

/**
 * @brief FADT generic Address
 * @struct fadt_gas_t
 *
 */
typedef struct PACKED
{
    uint8_t addressSpace;
    uint8_t bitWidth;
    uint8_t bitOffset;
    uint8_t accessSize;
    uint64_t address;
} fadt_gas_t;

/**
 * @brief Fixed ACPI Description Table
 * @struct fadt_t
 *
 */
typedef struct PACKED
{
    acpi_header_t header;
    uint32_t firmwareControl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferredPowerManagementProfile;
    uint16_t sciInterrupt;
    uint32_t smiCommandPort;
    uint8_t acpiEnable;
    uint8_t acpiDisable;
    uint8_t s4BiosReq;
    uint8_t pstateControl;
    uint32_t pm1aEventBlock;
    uint32_t pm1bEventBlock;
    uint32_t pm1aControlBlock;
    uint32_t pm1bControlBlock;
    uint32_t pm2ControlBlock;
    uint32_t pmTimerBlock;
    uint32_t gpe0Block;
    uint32_t gpe1Block;
    uint8_t pm1EventLength;
    uint8_t pm1ControlLength;
    uint8_t pm2ControlLength;
    uint8_t pmTimerLength;
    uint8_t gpe0Length;
    uint8_t gpe1Length;
    uint8_t gpe1Base;
    uint8_t cStateControl;
    uint16_t worstC2Latency;
    uint16_t worstC3Latency;
    uint16_t flushSize;
    uint16_t flushStride;
    uint8_t dutyOffset;
    uint8_t dutyWidth;
    uint8_t dayAlarm;
    uint8_t monthAlarm;
    uint8_t century;
    uint16_t bootArchFlags;
    uint8_t reserved2;
    uint32_t flags;
    fadt_gas_t resetReg;
    uint8_t resetValue;
    uint8_t reserved3[3];
    uint64_t xFirmwareControl;
    uint64_t xDsdt;
    fadt_gas_t xPm1aEventBlock;
    fadt_gas_t xPm1bEventBlock;
    fadt_gas_t xPm1aControlBlock;
    fadt_gas_t xPm1bControlBlock;
    fadt_gas_t xPm2ControlBlock;
    fadt_gas_t xPmTimerBlock;
    fadt_gas_t xGpe0Block;
    fadt_gas_t xGpe1Block;
} fadt_t;

/**
 * @brief Type safe way to get the FADT table
 *
 * @return fadt_t* The FADT table pointer
 */
#define FADT_GET() ((fadt_t*)acpi_tables_lookup("FACP", 0))

#define MADT_LAPIC 0
#define MADT_IOAPIC 1
#define MADT_INTERRUPT_OVERRIDE 2
#define MADT_NMI_SOURCE 3
#define MADT_LAPIC_NMI 4
#define MADT_LAPIC_ADDRESS_OVERRIDE 5

#define MADT_LAPIC_ENABLED (1 << 0)
#define MADT_LAPIC_ONLINE_CAPABLE (1 << 1)

#define MADT_FOR_EACH(madt, record) \
    for (record = (typeof(record))madt->records; (uint8_t*)record < (uint8_t*)madt + madt->header.length && \
        (uint8_t*)record + sizeof(madt_header_t) <= (uint8_t*)madt + madt->header.length && \
        (uint8_t*)record + record->header.length <= (uint8_t*)madt + madt->header.length; \
        record = (typeof(record))((uint8_t*)record + record->header.length))

typedef struct PACKED
{
    uint8_t type;
    uint8_t length;
} madt_header_t;

typedef struct PACKED
{
    madt_header_t header;
    uint8_t cpuId;
    uint8_t id;
    uint32_t flags;
} madt_lapic_t;

typedef struct PACKED
{
    madt_header_t header;
    uint8_t id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsiBase;
} madt_ioapic_t;

/**
 * @brief Multiple APIC Description Table
 * @struct madt_t
 */
typedef struct PACKED
{
    acpi_header_t header;
    uint32_t lapicAddress;
    uint32_t flags;
    madt_header_t records[];
} madt_t;

/**
 * @brief Type safe way to get the MADT table
 *
 * @return madt_t* The MADT table pointer
 */
#define MADT_GET() ((madt_t*)acpi_tables_lookup("APIC", 0))

/**
 * @brief Differentiated System Description Table
 * @struct dsdt_t
 */
typedef struct PACKED
{
    acpi_header_t header;
    uint8_t data[];
} dsdt_t;

/**
 * @brief Type safe way to get the DSDT table
 *
 * @return dsdt_t* The DSDT table pointer
 */
#define DSDT_GET() ((dsdt_t*)acpi_tables_lookup("DSDT", 0))

/**
 * @brief Load all ACPI tables
 *
 * @param xsdt The XSDT to load the tables from.
 * @return On success, the number of tables loaded. On failure, ERR.
 */
uint64_t acpi_tables_init(xsdt_t* xsdt);

/**
 * @brief Lookup the n'th table matching the signature.
 *
 * @param signature The signature of the table to look up.
 * @param n The index of the table to look up (0 indexed).
 * @return The table if found, NULL otherwise.
 */
acpi_header_t* acpi_tables_lookup(const char* signature, uint64_t n);

/** @} */

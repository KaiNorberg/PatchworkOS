#pragma once

#include "xsdt.h"

/**
 * @brief Fixed ACPI Description Table
 * @defgroup kernel_acpi_fadt FADT
 * @ingroup kernel_acpi
 * @{
 */

/**
 * @brief Enum for the `fadt_t::bootArchFlags` field.
 */
typedef enum
{
    FADT_BOOT_ARCH_PS2_EXISTS = (1 << 1)
} fadt_boot_arch_flags_t;

/**
 * @brief FADT generic Address Structure
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
 * @brief Fixed ACPI Description Table structure
 * @struct fadt_t
 *
 */
typedef struct PACKED
{
    sdt_t header;
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
 * @brief Less error prone way to get the FADT table
 *
 * @return fadt_t* The FADT table pointer
 */
#define FADT_GET() ((fadt_t*)xsdt_lookup("FACP", 0))

/** @} */

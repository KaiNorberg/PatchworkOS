#pragma once

#include <sys/proc.h>

#include "acpi/acpi.h"

/**
 * @brief High Precision Event Timer
 * @defgroup kernel_drivers_hpet HPET
 * @ingroup kernel_drivers
 *
 * The HPET is initalized via the ACPI sdt registration system.
 *
 * @see [OSDev HPET](https://wiki.osdev.org/HPET)
 *
 * @{
 */

/**
 * @brief HPET register offsets
 * @enum hpet_register_t
 */
typedef enum
{
    HPET_REG_GENERAL_CAPABILITIES_ID = 0x000,
    HPET_REG_GENERAL_CONFIG = 0x010,
    HPET_REG_GENERAL_INTERRUPT = 0x020,
    HPET_REG_MAIN_COUNTER_VALUE = 0x0F0,
    HPET_REG_TIMER0_CONFIG_CAP = 0x100,
    HPET_REG_TIMER0_COMPARATOR = 0x108,
} hpet_register_t;

/**
 * @brief The bit offset of the clock period in the capabilities register
 */
#define HPET_CAP_COUNTER_CLK_PERIOD_SHIFT 32

/**
 * @brief The bit to set to enable the HPET in the configuration register
 */
#define HPET_CONF_ENABLE_CNF_BIT (1 << 0)

/**
 * @brief The bit to set to enable legacy replacement mode in the configuration register
 */
#define HPET_CONF_LEG_RT_CNF_BIT (1 << 1)

/**
 * @brief If `hpet_t::addressSpaceId` is equal to this, the address is in system memory space.
 */
#define HPET_ADDRESS_SPACE_MEMORY 0

/**
 * @brief If `hpet_t::addressSpaceId` is equal to this, the address is in system I/O space.
 */
#define HPET_ADDRESS_SPACE_IO 1

/**
 * @brief High Precision Event Timer structure
 * @struct hpet_t
 */
typedef struct PACKED
{
    sdt_header_t header;
    uint8_t hardwareRevId;
    uint8_t comparatorCount : 5;
    uint8_t counterIs64Bit : 1;
    uint8_t reserved1 : 1;
    uint8_t legacyReplacementCapable : 1;
    uint16_t pciVendorId;
    uint8_t addressSpaceId;
    uint8_t registerBitWidth;
    uint8_t registerBitOffset;
    uint8_t reserved2;
    uint64_t address;
    uint8_t hpetNumber;
    uint16_t minimumTick;
    uint8_t pageProtection;
} hpet_t;

/**
 * @brief Retrieve the number of nanoseconds per HPET tick
 *
 * If the HPET is not initialized, this function will return 0.
 *
 * @return Nanoseconds per tick
 */
uint64_t hpet_nanoseconds_per_tick(void);

/**
 * @brief Read the current value of the HPET main counter
 *
 * If the HPET is not initialized, this function will return 0.
 *
 * @return Current current value of the HPET main counter in ticks
 */
uint64_t hpet_read_counter(void);

/**
 * @brief Reset the HPET main counter to 0 and enable the HPET
 */
void hpet_reset_counter(void);

/**
 * @brief Write a value to an HPET register
 *
 * @param reg The register to write to
 * @param value The value to write
 */
void hpet_write(uint64_t reg, uint64_t value);

/**
 * @brief Read a value from an HPET register
 *
 * @param reg The register to read from
 * @return The value read from the register
 */
uint64_t hpet_read(uint64_t reg);

/**
 * @brief Wait for a specified number of nanoseconds using the HPET
 *
 * This function uses a busy-wait loop, meaning its very CPU inefficient, but its usefull during early
 * initialization or when you are unable to block the current thread.
 *
 * @param nanoseconds The number of nanoseconds to wait
 */
void hpet_wait(clock_t nanoseconds);

/** @} */

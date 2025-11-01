#pragma once

#include <kernel/acpi/acpi.h>

#include <sys/proc.h>

/**
 * @brief High Precision Event Timer
 * @defgroup kernel_drivers_hpet HPET
 * @ingroup kernel_drivers
 *
 * The HPET is initalized via the ACPI sdt registration system.
 *
 * Note that since the HPET might be 32bit it could overflow rather quickly, so we implement a system for checking
 * roughly when it will overflow and accumulate the counter into a 64 bit nanosecond counter.
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
 * @brief The number of femtoseconds in one second
 */
#define HPET_FEMTOSECONDS_PER_SECOND 1000000000000000ULL

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
 * @brief Read the current accumulated counter in nanoseconds
 *
 * If the HPET is not initialized, this function will return 0.
 *
 * @return Current currently accumulated counter in nanoseconds
 */
clock_t hpet_read_ns_counter(void);

/**
 * @brief Wait for a specified number of nanoseconds using the HPET
 *
 * If the HPET is not initialized, this function will panic.
 *
 * This function uses a busy-wait loop, meaning its very CPU inefficient, but its usefull during early
 * initialization or when you are unable to block the current thread.
 *
 * @param nanoseconds The number of nanoseconds to wait
 */
void hpet_wait(clock_t nanoseconds);

/** @} */

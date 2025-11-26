#pragma once

#include <time.h>

/**
 * @brief Real Time Clock
 * @defgroup kernel_drivers_rtc RTC
 * @ingroup kernel_drivers
 *
 * The RTC driver provides functions to read the current time from the CMOS RTC.
 *
 * TODO: Move to module.
 *
 * @see [OSDev CMOS](https://wiki.osdev.org/CMOS)
 *
 * @{
 */

/**
 * @brief CMOS address port.
 */
#define CMOS_ADDRESS 0x70

/**
 * @brief CMOS data port.
 */
#define CMOS_DATA 0x71

/**
 * @brief Reads the current time from the RTC.
 *
 * @param time Pointer to a `struct tm` to store the current time. If NULL, the function does nothing.
 */
void rtc_read(struct tm* time);

/** @} */

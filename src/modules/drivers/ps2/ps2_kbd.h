#pragma once

#include "ps2.h"

/**
 * @brief PS/2 Keyboard Driver.
 * @defgroup module_drivers_ps2_kbd PS/2 Keyboard Driver
 * @ingroup modules_drivers_ps2
 *
 * @{
 */

/**
 * @brief State for the PS/2 keyboard interrupt handler.
 */
typedef struct
{
    bool isExtended; // True if the current packet contains PS2_DEV_RESPONSE_KBD_EXTENDED.
    bool isRelease;  // True if the current packet contains PS2_DEV_RESPONSE_KBD_RELEASE.
} ps2_kbd_irq_context_t;

/**
 * @brief Initialize a PS/2 keyboard device.
 *
 * @param info Device information structure.
 * @return 0 on success, ERR on failure.
 */
uint64_t ps2_kbd_init(ps2_device_info_t* info);

/** @} */

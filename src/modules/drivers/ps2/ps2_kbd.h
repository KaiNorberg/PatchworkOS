#pragma once

#include "ps2.h"

#include <kernel/abstract/kbd.h>

/**
 * @brief PS/2 Keyboard Driver.
 * @defgroup module_drivers_ps2_kbd PS/2 Keyboard Driver
 * @ingroup modules_drivers_ps2
 *
 * @{
 */

/**
 * @brief PS/2 keyboard private data.
 * @struct ps2_kbd_data_t
 */
typedef struct
{
    bool isExtended; // True if the current packet contains `PS2_DEV_RESPONSE_KBD_EXTENDED`.
    bool isRelease;  // True if the current packet contains `PS2_DEV_RESPONSE_KBD_RELEASE`.
    kbd_t* kbd;
} ps2_kbd_data_t;

/**
 * @brief Initialize a PS/2 keyboard device.
 *
 * @param info Device information structure.
 * @return On success, `0`. On failure, `ERR`.
 */
uint64_t ps2_kbd_init(ps2_device_info_t* info);

/**
 * @brief Register the IRQ handler for a PS/2 keyboard device.
 *
 * @param info Device information structure.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t ps2_kbd_irq_register(ps2_device_info_t* info);

/**
 * @brief Deinitialize a PS/2 keyboard device.
 *
 * @param info Device information structure.
 */
void ps2_kbd_deinit(ps2_device_info_t* info);

/** @} */

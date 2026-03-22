#pragma once

#include "ps2.h"

#include <kernel/drivers/abstract/kbd.h>

/**
 * @brief PS/2 Keyboard Driver.
 * @defgroup module_drivers_ps2_kbd PS/2 Keyboard Driver
 * @ingroup kernel_drivers_ps2
 *
 * @{
 */

/**
 * @brief PS/2 keyboard flags.
 * @enum ps2_kbd_flags_t
 */
typedef enum
{
    PS2_KBD_NONE = 0,
    PS2_KBD_EXTENDED = 1 << 0,
    PS2_KBD_RELEASE = 1 << 1,
} ps2_kbd_flags_t;

/**
 * @brief PS/2 keyboard private data.
 * @struct ps2_kbd_t
 */
typedef struct
{
    ps2_kbd_flags_t flags;
    kbd_t kbd;
} ps2_kbd_t;

/**
 * @brief Initialize a PS/2 keyboard device.
 *
 * @param info Device information structure.
 * @return An appropriate status value.
 */
status_t ps2_kbd_init(ps2_device_info_t* info);

/**
 * @brief Register the IRQ handler for a PS/2 keyboard device.
 *
 * @param info Device information structure.
 * @return An appropriate status value.
 */
status_t ps2_kbd_irq_register(ps2_device_info_t* info);

/**
 * @brief Deinitialize a PS/2 keyboard device.
 *
 * @param info Device information structure.
 */
void ps2_kbd_deinit(ps2_device_info_t* info);

/** @} */

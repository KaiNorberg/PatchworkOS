#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/kbd.h>

/**
 * @brief PS/2 Scanmap.
 * @defgroup kernel_drivers_ps2_scanmap PS/2 Scanmap
 * @ingroup kernel_drivers_ps2
 *
 * @{
 */

/**
 * @brief PS/2 scan code set to use
 */
#define PS2_SCAN_CODE_SET 2

/**
 * @brief PS/2 scancode
 */
typedef uint8_t ps2_scancode_t;

/**
 * @brief Convert a PS/2 scancode to a generic keycode
 *
 * @param scancode PS/2 scancode structure
 * @param isExtended Whether to use the extended scanmap
 * @return Corresponding generic keycode
 */
keycode_t ps2_scancode_to_keycode(ps2_scancode_t scancode, bool isExtended);

/** @} */

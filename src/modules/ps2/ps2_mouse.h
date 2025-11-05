#pragma once

#include "ps2.h"

/**
 * @brief PS/2 Mouse Driver.
 * @defgroup kernel_drivers_ps2_mouse PS/2 Mouse Driver
 * @ingroup modules_ps2
 *
 * TODO: Implement scrolling and buttons 4 and 5.
 *
 * @{
 */

/**
 * @brief PS/2 mouse packet flags
 */
typedef enum
{
    PS2_PACKET_BUTTON_LEFT = (1 << 0),
    PS2_PACKET_BUTTON_RIGHT = (1 << 1),
    PS2_PACKET_BUTTON_MIDDLE = (1 << 2),
    PS2_PACKET_ALWAYS_ONE = (1 << 3),
    PS2_PACKET_X_SIGN = (1 << 4),
    PS2_PACKET_Y_SIGN = (1 << 5),
    PS2_PACKET_X_OVERFLOW = (1 << 6),
    PS2_PACKET_Y_OVERFLOW = (1 << 7),
} ps2_mouse_packet_flags_t;

/**
 * @brief PS/2 mouse packet structure
 *
 * The packet is received one member at a time from top to bottom.
 */
typedef struct ps2_mouse_packet
{
    ps2_mouse_packet_flags_t flags; ///< Packet flags
    int16_t deltaX;                 ///< X-axis movement
    int16_t deltaY;                 ///< Y-axis movement
} ps2_mouse_packet_t;

/**
 * @brief PS/2 mouse packet index
 *
 * Specifies which member is the next byte to be received.
 */
typedef enum
{
    PS2_PACKET_FLAGS = 0,
    PS2_PACKET_DELTA_X = 1,
    PS2_PACKET_DELTA_Y = 2,
} ps2_mouse_packet_index_t;

/**
 * @brief PS/2 mouse IRQ context
 *
 * Holds state for mouse interrupt handling.
 */
typedef struct ps2_mouse_irq_context
{
    ps2_mouse_packet_index_t index; ///< Current packet byte index.
    ps2_mouse_packet_t packet;      ///< Current packet being assembled
} ps2_mouse_irq_context_t;

/**
 * @brief Initialize a PS/2 mouse device
 *
 * @param info Device information structure
 * @return 0 on success, ERR on failure
 */
uint64_t ps2_mouse_init(ps2_device_info_t* info);

/** @} */

#pragma once

#include "ps2.h"

#include <kernel/drivers/abstractions/mouse.h>

/**
 * @brief PS/2 Mouse Driver.
 * @defgroup module_drivers_ps2_mouse PS/2 Mouse Driver
 * @ingroup modules_drivers_ps2
 *
 * TODO: Implement scrolling and buttons 4 and 5.
 *
 * @{
 */

/**
 * @brief PS/2 mouse packet flags.
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
 * @brief PS/2 mouse packet structure.
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
 * @brief PS/2 mouse packet index.
 *
 * Since the packet is received one byte at a time, this enum specifies which member is the next byte to be received.
 */
typedef enum
{
    PS2_PACKET_FLAGS = 0,
    PS2_PACKET_DELTA_X = 1,
    PS2_PACKET_DELTA_Y = 2,
} ps2_mouse_packet_index_t;

/**
 * @brief PS/2 mouse private data.
 * @struct ps2_mouse_data_t
 *
 */
typedef struct
{
    ps2_mouse_packet_index_t index; ///< Current packet byte index.
    ps2_mouse_packet_t packet;      ///< Current packet being assembled
    mouse_t* mouse;
} ps2_mouse_data_t;

/**
 * @brief Initialize a PS/2 mouse device.
 *
 * @param info Device information structure
 * @return On success, `0`. On failure, `ERR`.
 */
uint64_t ps2_mouse_init(ps2_device_info_t* info);

/**
 * @brief Deinitialize a PS/2 mouse device.
 *
 * @param info Device information structure.
 */
void ps2_mouse_deinit(ps2_device_info_t* info);

/** @} */

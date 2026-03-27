#pragma once

#include <kernel/fs/devfs.h>
#include <kernel/fs/stringstream.h>
#include <kernel/sched/wait.h>

#include <libstd/kbd.h>
#include <libstd/proc.h>
#include <stdint.h>

/**
 * @brief Mouse driver abstraction.
 * @defgroup kernel_drivers_abstract_mouse Mouse Abstraction
 * @ingroup kernel_drivers_abstract
 *
 * Mouse devices are exposed as a `/dev/mouse/[id]/` directory, containing the below files.
 *
 * ## name
 *
 * A read-only file that contains the driver defined name of the mouse device.
 *
 * ## events
 *
 * A readable and pollable file that provides a stream of mouse events, where each event is defined as
 *
 * ```
 * [sign][value][type]
 * ```
 *
 * where `sign` is either `+` or `-`, `value` is a 3 digit integer, and `type` is a single character where `_`
 * represents a button press, `^` represents a button release, `x` represents movement in the X direction, `y`
 * represents movement in the Y direction, and `z` represents scrolling.
 *
 * The below example shows a press of the left mouse button, moving the mouse `10` units in the X direction and `-5`
 * units in the Y direction, releasing the left mouse button, and then scrolling `3` units.
 *
 * ```
 * +001_+010x-005y+001^+003z
 * ```
 *
 * @note The format is specified such that if `scan()` is used with "%u%c" the `scan()` call does not require any
 * "ungets".
 *
 * @{
 */

/**
 * @brief Mouse structure.
 * @struct mouse_t
 */
typedef struct
{
    const char* name;
    struct
    {
        dentry_t* dir;
        list_t files;
        stringstream_t stream;
    } internal;
} mouse_t;

/**
 * @brief Initialize the mouse abstraction.
 */
void mouse_init(void);

/**
 * @brief Register a new mouse.
 *
 * @param mouse Pointer to the mouse structure to initialize.
 * @return An appropriate status value.
 */
status_t mouse_register(mouse_t* mouse);

/**
 * @brief Unregister a mouse.
 *
 * @param mouse The mouse to unregister.
 */
void mouse_unregister(mouse_t* mouse);

/**
 * @brief Push a mouse button press event to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param button Button to press.
 */
void mouse_press(mouse_t* mouse, uint8_t button);

/**
 * @brief Push a mouse button release event to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param button Button to release.
 */
void mouse_release(mouse_t* mouse, uint8_t button);

/**
 * @brief Push a mouse movement in the X direction to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param delta Amount to move in the X direction.
 */
void mouse_move_x(mouse_t* mouse, int8_t delta);

/**
 * @brief Push a mouse movement in the Y direction to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param delta Amount to move in the Y direction.
 */
void mouse_move_y(mouse_t* mouse, int8_t delta);

/**
 * @brief Push a mouse scroll event to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param delta Amount to scroll.
 */
void mouse_scroll(mouse_t* mouse, int8_t delta);

/** @} */

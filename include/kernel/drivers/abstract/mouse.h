#pragma once

#include <kernel/fs/devfs.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/fifo.h>

#include <stdint.h>
#include <sys/kbd.h>
#include <sys/proc.h>

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
 * A readable and pollable file that provides a stream of mouse events represented as integer values suffixed with a
 * single character indicating the type of the event.
 *
 * The ´x` and `y` characters indicate movement in the X and Y directions respectively, the `_` and `^` represent a
 * button press and release respectively, and the `z` character represents a scroll event.
 *
 * Buttons are represented as their button number starting from `1` where `1` is the left button, `2` is the right
 * button, and `3` is the middle button.
 *
 * The below example shows a press of the left mouse button, moving the mouse `10` units in the X direction and `-5`
 * units in the Y direction, releasing the left mouse button, and then scrolling `3` units.
 *
 * ```
 * 1_10x-5y1^3z
 * ```
 *
 * If no events are available to read, the read call will block until an event is available unless the file is opened in
 * non-blocking mode in which case the read will fail with `EAGAIN`.
 *
 * @note The format is specified such that if `scan()` is used with "%u%c" the `scan()` call does not require any
 * "ungets".
 *
 * @{
 */

/**
 * @brief Size of the mouse client buffer.
 */
#define MOUSE_CLIENT_BUFFER_SIZE 512

/**
 * @brief Keyboard event client structure.
 * @struct mouse_client_t
 */
typedef struct mouse_client
{
    list_entry_t entry;
    fifo_t fifo;
    uint8_t buffer[MOUSE_CLIENT_BUFFER_SIZE];
} mouse_client_t;

/**
 * @brief Mouse structure.
 * @struct mouse_t
 */
typedef struct
{
    const char* name;
    wait_queue_t waitQueue;
    list_t clients;
    lock_t lock;
    dentry_t* dir;
    list_t files;
} mouse_t;

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
void mouse_press(mouse_t* mouse, uint32_t button);

/**
 * @brief Push a mouse button release event to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param button Button to release.
 */
void mouse_release(mouse_t* mouse, uint32_t button);

/**
 * @brief Push a mouse movement in the X direction to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param delta Amount to move in the X direction.
 */
void mouse_move_x(mouse_t* mouse, int64_t delta);

/**
 * @brief Push a mouse movement in the Y direction to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param delta Amount to move in the Y direction.
 */
void mouse_move_y(mouse_t* mouse, int64_t delta);

/**
 * @brief Push a mouse scroll event to the mouse event queue.
 *
 * @param mouse Pointer to the mouse structure.
 * @param delta Amount to scroll.
 */
void mouse_scroll(mouse_t* mouse, int64_t delta);

/** @} */

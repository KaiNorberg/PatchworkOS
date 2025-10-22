#pragma once

#include "fs/sysfs.h"
#include "sched/wait.h"

#include <stdint.h>
#include <sys/mouse.h>

/**
 * @brief Mouse driver abstraction.
 * @defgroup kernel_drivers_abstractions_mouse Mouse Abstraction
 * @ingroup kernel_drivers_abstractions
 *
 * Mouse devices are exposed as `/dev/mouse/[id]` directories, containing the following files:
 * - `events`: A read-only pollable file that can be read to receive mouse events as `mouse_event_t` structs.
 * - `name`: A read-only file that contains the mouse driver specified name (e.g. "PS/2")
 *
 * @{
 */

/**
 * @brief Maximum number of queued mouse events.
 */
#define MOUSE_MAX_EVENT 32

/**
 * @brief Mouse structure.
 * @struct mouse_t
 */
typedef struct
{
    char name[MAX_NAME];
    mouse_event_t events[MOUSE_MAX_EVENT];
    uint64_t writeIndex;
    wait_queue_t waitQueue;
    lock_t lock;
    dentry_t* dir;
    dentry_t* eventsFile;
    dentry_t* nameFile;
} mouse_t;

/**
 * @brief Allocate and initialize a mouse structure.
 *
 * Will make the mouse available under `/dev/mouse/[id]`.
 *
 * @param name Driver specified name of the mouse device.
 * @return On success, the new mouse structure. On failure, `NULL` and `errno` is set.
 */
mouse_t* mouse_new(const char* name);

/**
 * @brief Free and deinitialize a mouse structure.
 *
 * Removes the mouse from `/dev/mouse/[id]`.
 *
 * @param mouse Pointer to the mouse structure to free.
 */
void mouse_free(mouse_t* mouse);

/**
 * @brief Push a new mouse event to the mouse event queue.
 *
 * The event will be made available to user space by reading the `stream` file.
 *
 * @param mouse Pointer to the mouse structure.
 * @param buttons The button state.
 * @param deltaX The change in X position.
 * @param deltaY The change in Y position.
 */
void mouse_push(mouse_t* mouse, mouse_buttons_t buttons, int64_t deltaX, int64_t deltaY);

/** @} */

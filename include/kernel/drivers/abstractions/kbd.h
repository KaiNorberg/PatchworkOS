#pragma once

#include <kernel/fs/sysfs.h>
#include <kernel/sched/wait.h>

#include <stdint.h>
#include <sys/kbd.h>

/**
 * @brief Keyboard abstraction.
 * @defgroup kernel_drivers_abstractions_kbd Keyboard Abstraction
 * @ingroup kernel_drivers_abstractions
 *
 * Keyboard devices are exposed as `/dev/kbd/[id]` directories, containing the following files:
 * - `events`: A read-only pollable file that can be read to receive keyboard events as `kbd_event_t` structs.
 * - `name`: A read-only file that contains the keyboard driver specified name (e.g. "PS/2")
 *
 * @{
 */

/**
 * @brief Maximum number of queued keyboard events.
 */
#define KBD_MAX_EVENT 32

/**
 * @brief Keyboard structure.
 * @struct kbd_t
 */
typedef struct
{
    char* name;
    kbd_event_t events[KBD_MAX_EVENT];
    uint64_t writeIndex;
    kbd_mods_t mods;
    wait_queue_t waitQueue;
    lock_t lock;
    dentry_t* dir;
    dentry_t* eventsFile;
    dentry_t* nameFile;
} kbd_t;

/**
 * @brief Allocate and initialize a keyboard structure.
 *
 * Will make the keyboard available under `/dev/kbd/[id]`.
 *
 * @param name Driver specified name of the keyboard device.
 * @return On success, the new keyboard structure. On failure, `NULL` and `errno` is set.
 */
kbd_t* kbd_new(const char* name);

/**
 * @brief Free and deinitialize a keyboard structure.
 *
 * Removes the keyboard from `/dev/kbd/[id]`.
 *
 * @param kbd Pointer to the keyboard structure to free.
 */
void kbd_free(kbd_t* kbd);

/**
 * @brief Push a keyboard event to the keyboard event queue.
 *
 * The event will be made available to user space by reading the `stream` file.
 *
 * @param kbd Pointer to the keyboard structure.
 * @param type The type of the keyboard event.
 * @param code The keycode of the keyboard event.
 */
void kbd_push(kbd_t* kbd, kbd_event_type_t type, keycode_t code);

/** @} */

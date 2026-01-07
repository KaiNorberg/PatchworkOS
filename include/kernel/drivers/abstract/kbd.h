#pragma once

#include <kernel/fs/devfs.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/ring.h>

#include <stdint.h>
#include <sys/kbd.h>
#include <sys/proc.h>

/**
 * @brief Keyboard abstraction.
 * @defgroup kernel_drivers_abstract_kbd Keyboard Abstraction
 * @ingroup kernel_drivers_abstract
 *
 * Keyboard devices are exposed as a `/dev/kbd/[id]/` directory, containing the below files.
 *
 * ## name
 *
 * A read-only file that contains the driver defined name of the keyboard device.
 *
 * ## stream
 *
 * A readable file that provides a stream of keyboard events represented as new line deliminated keycodes prefixed with
 * either `^` or `_` if the event is a key release or press respectively.
 *
 * For example, the stream:
 *
 * ```
 * _30
 * ^30
 * _5
 * ```
 *
 * represents a press of the `1` key, its subsequent release, and then a press of the `A` key.
 *
 * If no events are available to read, the read call will block until an event is available unless the file is opened in
 * non-blocking mode in which case the read will fail with `EAGAIN`.
 *
 * @see libstd_sys_kbd for keycode definitions.
 *
 * @{
 */

/**
 * @brief Keyboard structure.
 * @struct kbd_t
 */
typedef struct
{
    char name[MAX_PATH];
    uint8_t buffer[PAGE_SIZE];
    ring_t events;
    wait_queue_t waitQueue;
    lock_t lock;
    dentry_t* dir;
    list_t files;
} kbd_t;

/**
 * @brief Allocate and initialize a new keyboard.
 *
 * @param name The driver specified name of the keyboard.
 * @return On success, the new keyboard. On failure, `NULL` and `errno` is set.
 */
kbd_t* kbd_new(const char* name);

/**
 * @brief Frees a keyboard.
 *
 * @param kbd The keyboard to free.
 */
void kbd_free(kbd_t* kbd);

/**
 * @brief Push a keyboard press event to the keyboard event queue.
 *
 * @param kbd The keyboard.
 * @param type The type of event.
 * @param code The keycode of the event.
 */
void kbd_press(kbd_t* kbd, keycode_t code);

/**
 * @brief Push a keyboard release event to the keyboard event queue.
 *
 * @param kbd The keyboard.
 * @param code The keycode to release.
 */
void kbd_release(kbd_t* kbd, keycode_t code);

/** @} */

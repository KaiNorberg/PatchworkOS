#pragma once

#include <kernel/fs/devfs.h>
#include <kernel/fs/stringstream.h>
#include <kernel/sched/wait.h>

#include <libstd/kbd.h>
#include <libstd/proc.h>
#include <stdint.h>

typedef struct kbd kbd_t;

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
 * ## events
 *
 * A readable and pollable file that provides a stream of keyboard events, where each event is defined as
 *
 * ```
 * [keycode][action]
 * ```
 *
 * where `keycode` is a 3 digit integer and `action` is a single character where `_` represents a key press and `^`
 * represents a key release.
 *
 * The below example shows a press of the '1' key, followed by its release, and then a press of the 'A' key.
 *
 * ```
 * 030_030^005_
 * ```
 *
 * @see libstd_kbd for keycode definitions.
 *
 * @{
 */

/**
 * @brief Keyboard structure.
 * @struct kbd_t
 */
typedef struct kbd
{
    const char* name;
    struct
    {
        dentry_t* dir;
        list_t files;
        stringstream_t stream;
    } internal;
} kbd_t;

/**
 * @brief Initialize the keyboard abstraction.
 */
void kbd_init(void);

/**
 * @brief Register a new keyboard.
 *
 * @param kbd Pointer to the keyboard structure to initialize.
 * @return An appropriate status value.
 */
status_t kbd_register(kbd_t* kbd);

/**
 * @brief Unregister a keyboard.
 *
 * @param kbd The keyboard to unregister.
 */
void kbd_unregister(kbd_t* kbd);

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

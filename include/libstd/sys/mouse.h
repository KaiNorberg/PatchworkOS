#ifndef _SYS_MOUSE_H
#define _SYS_MOUSE_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/clock_t.h"

/**
 * @brief Mouse device header.
 * @ingroup libstd
 * @defgroup libstd_sys_mouse Mouse device
 *
 * The `sys/mouse.h` header defines structs and constants used by mouse devices for example `/dev/mouse/ps2`. The
 * primary way to use a mouse device is to open it and then read from it to retrieve `mouse_event_t` structures.
 *
 */

/**
 * @brief Mouse buttons enum.
 * @ingroup libstd_sys_mouse
 *
 * The `mouse_buttons_t` enum is used to store the state of mouse buttons.
 *
 */
typedef enum
{
    MOUSE_NONE = 0,          //!< None
    MOUSE_RIGHT = (1 << 0),  //!< Right mouse button
    MOUSE_MIDDLE = (1 << 1), //!< Middle mouse button
    MOUSE_LEFT = (1 << 2)    //!< Left mouse button
} mouse_buttons_t;

/**
 * @brief Mouse event structure.
 * @ingroup libstd_sys_mouse
 *
 * The `mouse_event_t` structure can be read from mouse files, for example `/dev/mouse/ps2`. Mouse files will block
 * until a mouse event happens, mouse files will never return partial events.
 */
typedef struct
{
    clock_t time;            //!< Clock ticks since boot when the event happened
    mouse_buttons_t buttons; //!< Which buttons were held down durring the event
    int64_t deltaX;          //!< Change in X coordinate
    int64_t deltaY;          //!< Change in Y coordinate
} mouse_event_t;

#if defined(__cplusplus)
}
#endif

#endif

#ifndef _SYS_MOUSE_H
#define _SYS_MOUSE_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/clock_t.h"

typedef enum
{
    MOUSE_NONE = 0,
    MOUSE_RIGHT = (1 << 0),
    MOUSE_MIDDLE = (1 << 1),
    MOUSE_LEFT = (1 << 2)
} mouse_buttons_t;

/**
 * @brief Mouse event structure.
 *
 * The `mouse_event_t` structure can be read from mouse files, for example `sys:/mouse/ps2`. Mouse files will block
 * until a mouse event happens, mouse files will never return partial events.
 */
typedef struct mouse_event
{
    clock_t time;
    mouse_buttons_t buttons;
    int64_t deltaX;
    int64_t deltaY;
} mouse_event_t;

#if defined(__cplusplus)
}
#endif

#endif

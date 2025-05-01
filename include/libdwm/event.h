#ifndef DWM_EVENT_H
#define DWM_EVENT_H 1

#include "surface.h"
#include "point.h"

#include <stdint.h>
#include <sys/kbd.h>
#include <sys/mouse.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef uint16_t event_type_t;

// Dwm events, send from the dwm.
#define EVENT_SCREEN_INFO 0
#define EVENT_INIT 1
#define EVENT_KBD 3
#define EVENT_MOUSE 4
#define EVENT_FOCUS_IN 5
#define EVENT_FOCUS_OUT 6

// Library events, defined by libdwm.
#define LEVENT_BASE (1 << 14)
#define LEVENT_FREE (LEVENT_BASE + 1) // May be recieved outside of a dispatch call.
#define LEVENT_ACTION (LEVENT_BASE + 2)
#define LEVENT_REDRAW (LEVENT_BASE + 3)

// User events, defined by individual programs
#define UEVENT_BASE (1 << 15)

// Structs stored in event_t.data, note that some events have no data.
typedef struct
{
    uint64_t width;
    uint64_t height;
} event_screen_info_t;

typedef struct
{
    kbd_event_type_t type;
    kbd_mods_t mods;
    keycode_t code;
    char ascii;
} event_kbd_t;

typedef struct
{
    mouse_buttons_t held;
    mouse_buttons_t pressed;
    mouse_buttons_t released;
    point_t pos;
    point_t delta;
} event_mouse_t;

typedef struct event
{
    event_type_t type;
    surface_id_t target;
    union
    {
        event_screen_info_t screenInfo;
        event_kbd_t kbd;
        event_mouse_t mouse;
        uint8_t raw[64];
    };
} event_t;

#if defined(__cplusplus)
}
#endif

#endif

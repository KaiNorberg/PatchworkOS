#ifndef DWM_EVENT_H
#define DWM_EVENT_H 1

#include "element_id.h"
#include "font_id.h"
#include "point.h"
#include "rect.h"
#include "surface.h"

#include <stdint.h>
#include <sys/kbd.h>
#include <sys/mouse.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef uint16_t event_type_t;

// Dwm events, send by the dwm.
#define EVENT_SCREEN_INFO 0
#define EVENT_KBD 1
#define EVENT_MOUSE 2
#define EVENT_FOCUS_IN 3
#define EVENT_FOCUS_OUT 4
#define EVENT_FONT_NEW 5
#define EVENT_FONT_INFO 6
#define EVENT_SURFACE_MOVE 7
#define EVENT_TIMER 8
#define EVENT_SCREEN_ACQUIRE 9

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
    point_t screenPos;
    point_t delta;
} event_mouse_t;

typedef struct
{
    font_id_t id;
    uint64_t width;
    uint64_t height;
} event_font_new_t;

typedef struct
{
    font_id_t id;
    uint64_t width;
    uint64_t height;
} event_font_info_t;

typedef struct
{
    rect_t rect;
} event_surface_move_t;

typedef struct
{
    uint64_t index;
} event_screen_acquire_t;

// Library events, sent by libdwm.
#define LEVENT_BASE (1 << 14)
#define LEVENT_INIT (LEVENT_BASE + 1)
#define LEVENT_FREE (LEVENT_BASE + 2) // May be recieved outside of a dispatch call.
#define LEVENT_REDRAW (LEVENT_BASE + 3)
#define LEVENT_ACTION (LEVENT_BASE + 4)
#define LEVENT_QUIT (LEVENT_BASE + 5)

typedef struct
{
    element_id_t id;
} levent_init_t;

typedef struct
{
    element_id_t id;
    uint8_t propagate;
} levent_redraw_t;

typedef enum
{
    ACTION_NONE = 0,
    ACTION_RELEASE,
    ACTION_PRESS,
} action_type_t;

typedef struct
{
    element_id_t source;
    action_type_t type;
} levent_action_t;

// User events, defined by individual programs
#define UEVENT_BASE (1 << 15)

#define EVENT_MAX_DATA 64

typedef struct event
{
    event_type_t type;
    surface_id_t target;
    union {
        event_screen_info_t screenInfo;
        event_kbd_t kbd;
        event_mouse_t mouse;
        event_font_new_t fontNew;
        event_font_info_t fontInfo;
        event_surface_move_t surfaceMove;
        event_screen_acquire_t screenAcquire;
        levent_init_t lInit;
        levent_redraw_t lRedraw;
        levent_action_t lAction;
        uint8_t raw[EVENT_MAX_DATA];
    };
} event_t;

#if defined(__cplusplus)
}
#endif

#endif

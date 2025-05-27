#ifndef PATCHWORK_EVENT_H
#define PATCHWORK_EVENT_H 1

#include "element_id.h"
#include "point.h"
#include "rect.h"
#include "surface.h"

#include <stdint.h>
#include <sys/io.h>
#include <sys/kbd.h>
#include <sys/mouse.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef uint16_t event_type_t;

// Dwm events, send by the dwm.
#define EVENT_SCREEN_INFO 0
#define EVENT_SURFACE_NEW 1
#define EVENT_KBD 2
#define EVENT_MOUSE 3
#define EVENT_FOCUS_IN 4
#define EVENT_FOCUS_OUT 5
#define EVENT_SURFACE_MOVE 6
#define EVENT_TIMER 7
#define EVENT_CURSOR_ENTER 8
#define EVENT_CURSOR_LEAVE 9

typedef struct
{
    uint64_t width;
    uint64_t height;
} event_screen_info_t;

typedef struct
{
    char shmem[MAX_NAME];
} event_surface_new_t;

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
    rect_t rect;
} event_surface_move_t;

typedef struct
{
    uint64_t index;
} event_screen_acquire_t;

typedef event_mouse_t event_cursor_enter_t;
typedef event_mouse_t event_cursor_leave_t;

// Library events, sent by libpatchwork.
#define LEVENT_BASE (1 << 14)
#define LEVENT_INIT (LEVENT_BASE + 1)
#define LEVENT_FREE (LEVENT_BASE + 2) // May be received outside of a dispatch call.
#define LEVENT_REDRAW (LEVENT_BASE + 3)
#define LEVENT_ACTION (LEVENT_BASE + 4)
#define LEVENT_QUIT (LEVENT_BASE + 5)
#define LEVENT_FORCE_ACTION (LEVENT_BASE + 6)

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
    ACTION_CANCEL,
} action_type_t;

typedef struct
{
    element_id_t source;
    action_type_t type;
} levent_action_t;

typedef struct
{
    action_type_t action;
} levent_force_action_t;

// User events, defined by individual programs
#define UEVENT_BASE (1 << 15)

#define EVENT_MAX_DATA 64

typedef struct event
{
    event_type_t type;
    surface_id_t target;
    union {
        event_screen_info_t screenInfo;
        event_surface_new_t surfaceNew;
        event_kbd_t kbd;
        event_mouse_t mouse;
        event_surface_move_t surfaceMove;
        event_screen_acquire_t screenAcquire;
        event_cursor_enter_t cursorEnter;
        event_cursor_leave_t cursorLeave;
        levent_init_t lInit;
        levent_redraw_t lRedraw;
        levent_action_t lAction;
        levent_force_action_t lForceAction;
        uint8_t raw[EVENT_MAX_DATA];
    };
} event_t;

#if defined(__cplusplus)
}
#endif

#endif

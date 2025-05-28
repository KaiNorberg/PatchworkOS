#ifndef PATCHWORK_EVENT_H
#define PATCHWORK_EVENT_H 1

#include "element_id.h"
#include "point.h"
#include "rect.h"
#include "surface.h"

#include <assert.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/kbd.h>
#include <sys/mouse.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef uint16_t event_type_t;

// This bitmask decides what events will be received by the display.
// To allow for a maximum of 256 events we use a array of 4 uint64_t's.
// The bitflag for each event is the n'th bit where n is the event type. Note that the 80 bit would be the 80-64=16'th
// bit in the second uint64_t. Note that this only applies to events sent
// by the dwm, all other events are always received.
// By default events 0-63 inclusive are received (the first uint64_t is by deafult UINT64_MAX)
typedef uint64_t event_bitmask_t[4];

// Dwm events, send by the dwm.
#define EVENT_SCREEN_INFO 0
#define EVENT_SURFACE_NEW 1
#define EVENT_KBD 2
#define EVENT_MOUSE 3
#define EVENT_TIMER 4
#define EVENT_CURSOR_ENTER 5
#define EVENT_CURSOR_LEAVE 6
#define EVENT_REPORT 7 // Received whenever something about the surface is changed or from CMD_SURFACE_REPORT.

// Global events will be recieved from all surfaces, even if those surfaces were not created by the recieving client.
#define EVENT_GLOBAL_ATTACH 64
#define EVENT_GLOBAL_DETACH 65
#define EVENT_GLOBAL_REPORT 66
#define EVENT_GLOBAL_KBD 67
#define EVENT_GLOBAL_MOUSE 68

// The dwm promises to never implement more events then this.
#define EVENT_MAX (UINT8_MAX + 1)

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

typedef event_mouse_t event_cursor_enter_t;
typedef event_mouse_t event_cursor_leave_t;

// Stores what changed in the report. Note that there is no flag for the id and type as those never change.
typedef enum
{
    REPORT_NONE = 0,
    REPORT_RECT = 1 << 0,
    REPORT_IS_VISIBLE = 1 << 1,
    REPORT_IS_FOCUSED = 1 << 2,
    REPORT_NAME = 1 << 3,
} report_flags_t;

typedef struct
{
    report_flags_t flags;
    surface_info_t info;
} event_report_t;

typedef struct
{
    surface_info_t info;
} event_global_attach_t;

typedef struct
{
    surface_info_t info;
} event_global_detach_t;

typedef event_report_t event_global_report_t;

typedef event_kbd_t event_global_kbd_t;

typedef event_mouse_t event_global_mouse_t;

// Library events, sent by libpatchwork.
#define LEVENT_BASE EVENT_MAX
#define LEVENT_INIT (LEVENT_BASE + 1)
#define LEVENT_FREE (LEVENT_BASE + 2) // May be received outside of a dispatch call.
#define LEVENT_REDRAW (LEVENT_BASE + 3)
#define LEVENT_ACTION (LEVENT_BASE + 4)
#define LEVENT_QUIT (LEVENT_BASE + 5)
#define LEVENT_FORCE_ACTION (LEVENT_BASE + 6)

// The libpatchwork library promises to never implement more events then this.
#define LEVENT_MAX (1 << 9)

typedef struct
{
    element_id_t id;
} levent_init_t;

typedef struct
{
    element_id_t id;
    bool shouldPropagate;
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
    element_id_t dest;
    action_type_t action;
} levent_force_action_t;

// We leave some space between library events and user events for other potential event sources in the future.

// User events, defined by individual programs
#define UEVENT_BASE (1 << 15)

#define EVENT_MAX_DATA 128

typedef struct event
{
    event_type_t type;
    surface_id_t target;
    union {
        event_screen_info_t screenInfo;
        event_surface_new_t surfaceNew;
        event_kbd_t kbd;
        event_mouse_t mouse;
        event_cursor_enter_t cursorEnter;
        event_cursor_leave_t cursorLeave;
        event_report_t report;
        event_global_attach_t globalAttach;
        event_global_detach_t globalDetach;
        event_global_report_t globalReport;
        event_global_kbd_t globalKbd;
        event_global_mouse_t globalMouse;
        levent_init_t lInit;
        levent_redraw_t lRedraw;
        levent_action_t lAction;
        levent_force_action_t lForceAction;
        uint8_t raw[EVENT_MAX_DATA];
    };
} event_t;

static_assert(sizeof(event_t) == 144, "invalid event_t size");

#if defined(__cplusplus)
}
#endif

#endif

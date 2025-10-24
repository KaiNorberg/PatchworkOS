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

/**
 * @brief Desktop Window Manager Events
 * @defgroup libpatchwork_events Events
 * @ingroup libpatchwork
 *
 * @{
 */

/**
 * @brief Report flags.
 *
 * Used to specify what information changed in a report event.
 */
typedef enum
{
    REPORT_NONE = 0,
    REPORT_RECT = 1 << 0,
    REPORT_IS_VISIBLE = 1 << 1,
    REPORT_IS_FOCUSED = 1 << 2,
    REPORT_NAME = 1 << 3,
} report_flags_t;

/**
 * @brief Event type.
 *
 * Used to identify the type of an event.
 *
 * Events are divided into 4 categories:
 * - Standard events (0-63): Sent by the DWM to ONLY the display or surface that the event is targeted at, sent by
 * default.
 * - Global events (64-127): Sent by the DWM to all displays, not sent by default.
 * - Library events (128-191): Sent by the libpatchwork library to elements using the library, cant be subscribed to or
 * unsubscribed from.
 * - User events (192-255): Defined by individual programs, cant be subscribed to or unsubscribed from.
 */
typedef uint8_t event_type_t;

/**
 * @brief Event bitmask type.
 *
 * Used to decide what events will be received by a display.
 * By default events 0-63 inclusive are received (the first uint64_t is by deafult UINT64_MAX)
 */
typedef uint64_t event_bitmask_t[2];

#define EVENT_SCREEN_INFO 0
#define EVENT_SURFACE_NEW 1
#define EVENT_KBD 2
#define EVENT_MOUSE 3
#define EVENT_TIMER 4
#define EVENT_CURSOR_ENTER 5
#define EVENT_CURSOR_LEAVE 6
#define EVENT_REPORT 7

#define GEVENT_ATTACH 64
#define GEVENT_DETACH 65
#define GEVENT_REPORT 66
#define GEVENT_KBD 67
#define GEVENT_MOUSE 68

#define LEVENT_INIT 128
#define LEVENT_FREE 129
#define LEVENT_REDRAW 130
#define LEVENT_ACTION 131
#define LEVENT_QUIT 132
#define LEVENT_FORCE_ACTION 133

#define UEVENT_START 192
#define UEVENT_END 255

typedef struct
{
    uint64_t width;
    uint64_t height;
} event_screen_info_t;

typedef struct
{
    key_t shmemKey;
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

#ifdef static_assert
static_assert(sizeof(event_t) == 144, "invalid event_t size");
#endif

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

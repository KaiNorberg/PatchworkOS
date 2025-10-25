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
 * @brief Action type.
 *
 * Used to specify the type of an action event.
 */
typedef enum
{
    ACTION_NONE = 0,
    ACTION_RELEASE,
    ACTION_PRESS,
    ACTION_CANCEL,
} action_type_t;

/**
 * @brief Event type.
 *
 * Used to identify the type of an event.
 *
 * Events are divided into 4 categories:
 * - Standard events (0-127): Sent by the DWM to ONLY the display or surface that the event is targeted at, sent by
 * default.
 * - Global events (128-255): Sent by the DWM to all displays, not sent by default.
 * - Library events (256-1023): Sent by the libpatchwork library to elements using the library, cant be subscribed to or
 * unsubscribed from.
 * - User events (1024-65535): Defined by individual programs, cant be subscribed to or unsubscribed from.
 *
 * TODO: Global events are a security mess, when per-process namespaces stabilize we should consider if this could be
 * done better.
 */
typedef uint16_t event_type_t;

/**
 * @brief Event bitmask type.
 *
 * Used to decide what events will be received by a display, only applicable to events sent by the DWM.
 * By default events 0-127 inclusive are received (the first uint64_t is by default UINT64_MAX)
 */
typedef uint64_t event_bitmask_t[4];

#define EVENT_SCREEN_INFO 0
#define EVENT_SURFACE_NEW 1
#define EVENT_KBD 2
#define EVENT_MOUSE 3
#define EVENT_TIMER 4
#define EVENT_CURSOR_ENTER 5
#define EVENT_CURSOR_LEAVE 6
#define EVENT_REPORT 7

#define GEVENT_ATTACH 128
#define GEVENT_DETACH 129
#define GEVENT_REPORT 130
#define GEVENT_KBD 131
#define GEVENT_MOUSE 132

#define DWM_MAX_EVENT 256

#define LEVENT_INIT 256
#define LEVENT_FREE 257
#define LEVENT_REDRAW 258
#define LEVENT_ACTION 259
#define LEVENT_QUIT 260
#define LEVENT_FORCE_ACTION 261

#define UEVENT_START 1024
#define UEVENT_END 65535

/**
 * @brief Screen Info event.
 *
 * Sent as the response to the `CMD_SCREEN_INFO` command.
 */
typedef struct
{
    uint64_t width;
    uint64_t height;
} event_screen_info_t;

/**
 * @brief Surface New event.
 *
 * Sent as the response to the `CMD_SURFACE_NEW` command.
 */
typedef struct
{
    key_t shmemKey; ///< Key that can be `claim()`ed to access the surface's shared memory.
} event_surface_new_t;

/**
 * @brief Keyboard event.
 *
 * Sent when a key is pressed or released.
 */
typedef struct
{
    kbd_event_type_t type;
    kbd_mods_t mods;
    keycode_t code;
    char ascii;
} event_kbd_t;

/**
 * @brief Mouse event.
 *
 * Sent when the mouse is moved or a button is pressed or released.
 */
typedef struct
{
    mouse_buttons_t held;
    mouse_buttons_t pressed;
    mouse_buttons_t released;
    point_t pos;
    point_t screenPos;
    point_t delta;
} event_mouse_t;

/**
 * @brief Cursor Enter event.
 *
 * Sent when the cursor enters a surface.
 */
typedef event_mouse_t event_cursor_enter_t;

/**
 * @brief Cursor Leave event.
 *
 * Sent when the cursor leaves a surface.
 */
typedef event_mouse_t event_cursor_leave_t;

/**
 * @brief Report event.
 *
 * Sent when a surface's information changes.
 */
typedef struct
{
    report_flags_t flags;
    surface_info_t info;
} event_report_t;

/**
 * @brief Global Attach event.
 *
 * Sent when a display attaches to the DWM.
 */
typedef struct
{
    surface_info_t info;
} gevent_attach_t;

/**
 * @brief Global Detach event.
 *
 * Sent when a display detaches from the DWM.
 */
typedef struct
{
    surface_info_t info;
} gevent_detach_t;

/**
 * @brief Global Report event.
 *
 * Sent when any surface's information changes.
 */
typedef event_report_t gevent_report_t;

/**
 * @brief Global Keyboard event.
 *
 * Sent when a key is pressed or released regardless of which display is focused.
 */
typedef event_kbd_t gevent_kbd_t;

/**
 * @brief Global Mouse event.
 *
 * Sent when the mouse is moved or a button is pressed or released regardless of which display is focused or where
 * the cursor is.
 */
typedef event_mouse_t gevent_mouse_t;

/**
 * @brief Library Init event.
 *
 * Sent to an element when it is initialized.
 */
typedef struct
{
    element_id_t id;
} levent_init_t;

/**
 * @brief Library Redraw event.
 *
 * Sent to an element when it should redraw itself.
 */
typedef struct
{
    element_id_t id;
    bool shouldPropagate; ///< Whether the redraw event should be propagated to child elements.
} levent_redraw_t;

/**
 * @brief Library Action event.
 *
 * Sent to an element when an action occurs, for example a button element being clicked.
 */
typedef struct
{
    element_id_t source;
    action_type_t type;
} levent_action_t;

/**
 * @brief Library Force Action event.
 *
 * Sent to an element to force it to act as if an action occurred.
 */
typedef struct
{
    element_id_t dest;
    action_type_t action;
} levent_force_action_t;

/**
 * @brief Maximum size of event data.
 */
#define EVENT_MAX_DATA 128

/**
 * @brief Event structure.
 *
 * Represents an event sent by the DWM or libpatchwork.
 */
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
        gevent_attach_t globalAttach;
        gevent_detach_t globalDetach;
        gevent_report_t globalReport;
        gevent_kbd_t globalKbd;
        gevent_mouse_t globalMouse;
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

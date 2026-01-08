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
 * - Library events (256-511): Sent by the libpatchwork library to elements using the library, cant be subscribed to or
 * unsubscribed from.
 * - Internal Library events (512-1023): Used internally by libpatchwork, should not be used by programs.
 * - User events (1024-65535): Defined by individual programs, cant be subscribed to or unsubscribed from.
 *
 * @todo Global events are a security mess, when per-process namespaces stabilize we should consider if this could be
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

#define EVENT_GLOBAL_ATTACH 128
#define EVENT_GLOBAL_DETACH 129
#define EVENT_GLOBAL_REPORT 130
#define EVENT_GLOBAL_KBD 131
#define EVENT_GLOBAL_MOUSE 132

#define DWM_MAX_EVENT 256

#define EVENT_LIB_INIT 256
#define EVENT_LIB_DEINIT 257
#define EVENT_LIB_REDRAW 258
#define EVENT_LIB_ACTION 259
#define EVENT_LIB_QUIT 260
#define EVENT_LIB_FORCE_ACTION 261

#define EVENT_LIB_INTERNAL_WAKE 512

#define EVENT_USER_START 1024
#define EVENT_USER_END 65535

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
    char shmemKey[KEY_128BIT]; ///< Key that can be `claim()`ed to access the surface's shared memory.
} event_surface_new_t;

/**
 * @brief Keyboard event type.
 *
 */
typedef enum
{
    KBD_PRESS = 0,  ///< Key press event
    KBD_RELEASE = 1 ///< Key release event
} kbd_event_type_t;

/**
 * @brief Keyboard modifiers type.
 *
 */
typedef enum
{
    KBD_MOD_NONE = 0,       ///< No modifier
    KBD_MOD_CAPS = 1 << 0,  ///< Caps Lock modifier
    KBD_MOD_SHIFT = 1 << 1, ///< Shift modifier
    KBD_MOD_CTRL = 1 << 2,  ///< Control modifier
    KBD_MOD_ALT = 1 << 3,   ///< Alt modifier
    KBD_MOD_SUPER = 1 << 4, ///< Super (Windows/Command) modifier
} kbd_mods_t;

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
 * @brief Mouse buttons enum.
 *
 */
typedef enum
{
    MOUSE_NONE = 0,          ///< None
    MOUSE_LEFT = (1 << 1),    ///< Left mouse button
    MOUSE_RIGHT = (1 << 2),  ///< Right mouse button
    MOUSE_MIDDLE = (1 << 3), ///< Middle mouse button
} mouse_buttons_t;

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
} event_global_attach_t;

/**
 * @brief Global Detach event.
 *
 * Sent when a display detaches from the DWM.
 */
typedef struct
{
    surface_info_t info;
} event_global_detach_t;

/**
 * @brief Global Report event.
 *
 * Sent when any surface's information changes.
 */
typedef event_report_t event_global_report_t;

/**
 * @brief Global Keyboard event.
 *
 * Sent when a key is pressed or released regardless of which display is focused.
 */
typedef event_kbd_t event_global_kbd_t;

/**
 * @brief Global Mouse event.
 *
 * Sent when the mouse is moved or a button is pressed or released regardless of which display is focused or where
 * the cursor is.
 */
typedef event_mouse_t event_global_mouse_t;

/**
 * @brief Library Redraw event.
 *
 * Sent to an element when it should redraw itself.
 */
typedef struct
{
    element_id_t id;
    bool shouldPropagate; ///< Whether the redraw event should be propagated to child elements.
} event_lib_redraw_t;

/**
 * @brief Library Action event.
 *
 * Sent to an element when an action occurs, for example a button element being clicked.
 */
typedef struct
{
    element_id_t source;
    action_type_t type;
} event_lib_action_t;

/**
 * @brief Library Force Action event.
 *
 * Sent to an element to force it to act as if an action occurred.
 */
typedef struct
{
    element_id_t dest;
    action_type_t action;
} event_lib_force_action_t;

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
        event_global_attach_t globalAttach;
        event_global_detach_t globalDetach;
        event_global_report_t globalReport;
        event_global_kbd_t globalKbd;
        event_global_mouse_t globalMouse;
        event_lib_redraw_t libRedraw;
        event_lib_action_t libAction;
        event_lib_force_action_t libForceAction;
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

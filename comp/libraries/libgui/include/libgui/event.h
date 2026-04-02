#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgui/key.h>
#include <libgui/mouse.h>
#include <stdbool.h>

typedef struct gui_layout gui_layout_t;

/**
 * @brief Event definitions
 * @defgroup comp_libgui_event Events
 * @ingroup comp
 *
 * @{
 */

/**
 * @brief Widget event types.
 * @enum gui_event_type_t
 */
typedef enum
{
    GUI_EVENT_TYPE_MOUSE,
    GUI_EVENT_TYPE_MOUSE_ENTER,
    GUI_EVENT_TYPE_MOUSE_LEAVE,
    GUI_EVENT_TYPE_KEY,
    GUI_EVENT_TYPE_MOVE,
    GUI_EVENT_TYPE_COMMAND,
    GUI_EVENT_TYPE_FOCUS_IN,
    GUI_EVENT_TYPE_FOCUS_OUT,
} gui_event_type_t;

/**
 * @brief Widget event flags.
 * @enum gui_event_flags_t
 */
typedef enum
{
    GUI_EVENT_FLAG_NONE = 0,
    GUI_EVENT_FLAG_PROPAGATE = (1 << 0),
} gui_event_flags_t;

typedef enum
{
    GUI_EVENT_CMD_CLICK,
    GUI_EVENT_CMD_DOUBLE_CLICK,
    GUI_EVENT_CMD_VALUE_CHANGED,
    GUI_EVENT_CMD_TEXT_CHANGED,
    GUI_EVENT_CMD_SELECT,
} gui_event_cmd_t;

/**
 * @brief Widget event.
 * @struct gui_event_t
 */
typedef struct
{
    gui_event_type_t type;
    gui_event_flags_t flags;
    union {
        struct
        {
            int32_t x;
            int32_t y;
            int32_t z;
            gui_mouse_buttons_t pressed;
            gui_mouse_buttons_t released;
            gui_mouse_buttons_t held;
        } mouse;
        struct
        {
            keycode_t key;
            gui_key_state_t state;
            gui_key_mods_t modifiers;
        } key;
        struct
        {
            gfx_rect_t bounds;
        } move;
        struct
        {
            gui_widget_id_t source;
            gui_event_cmd_t command;
        } command;
        uint64_t _reserved[7];
    };
} gui_event_t;

/** @} */
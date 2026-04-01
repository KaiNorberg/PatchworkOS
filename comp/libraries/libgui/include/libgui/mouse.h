#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgui/button.h>
#include <stdbool.h>

typedef struct gui_layout gui_layout_t;

/**
 * @brief Mouse definitions
 * @defgroup comp_libgui_mouse Mouse
 * @ingroup comp
 *
 * @{
 */

/**
 * @brief Mouse buttons.
 * @enum gui_mouse_buttons_t
 */
typedef enum
{
    GUI_MOUSE_BUTTON_NONE = 0,
    GUI_MOUSE_BUTTON_LEFT = (1 << 0),
    GUI_MOUSE_BUTTON_RIGHT = (1 << 1),
    GUI_MOUSE_BUTTON_MIDDLE = (1 << 2),
} gui_mouse_buttons_t;

/**
 * @brief Mouse cursors.
 * @enum gui_mouse_cursor_t
 *
 * @see https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/Properties/cursor
 */
typedef enum
{
    GUI_MOUSE_CURSOR_NONE, ///< Will use parents cursor or default if it has not parent.
    GUI_MOUSE_CURSOR_DEFAULT,
    GUI_MOUSE_CURSOR_CONTEXT_MENU,
    GUI_MOUSE_CURSOR_HELP,
    GUI_MOUSE_CURSOR_POINTER,
    GUI_MOUSE_CURSOR_PROGRESS,
    GUI_MOUSE_CURSOR_WAIT,
    GUI_MOUSE_CURSOR_CELL,
    GUI_MOUSE_CURSOR_CROSSHAIR,
    GUI_MOUSE_CURSOR_TEXT,
    GUI_MOUSE_CURSOR_VERTICAL_TEXT,
    GUI_MOUSE_CURSOR_ALIAS,
    GUI_MOUSE_CURSOR_COPY,
    GUI_MOUSE_CURSOR_MOVE,
    GUI_MOUSE_CURSOR_NO_DROP,
    GUI_MOUSE_CURSOR_NOT_ALLOWED,
    GUI_MOUSE_CURSOR_GRAB,
    GUI_MOUSE_CURSOR_GRABBING,
    GUI_MOUSE_CURSOR_ALL_SCROLL,
    GUI_MOUSE_CURSOR_COL_RESIZE,
    GUI_MOUSE_CURSOR_ROW_RESIZE,
    GUI_MOUSE_CURSOR_N_RESIZE,
    GUI_MOUSE_CURSOR_E_RESIZE,
    GUI_MOUSE_CURSOR_S_RESIZE,
    GUI_MOUSE_CURSOR_W_RESIZE,
    GUI_MOUSE_CURSOR_NE_RESIZE,
    GUI_MOUSE_CURSOR_NW_RESIZE,
    GUI_MOUSE_CURSOR_SE_RESIZE,
    GUI_MOUSE_CURSOR_SW_RESIZE,
    GUI_MOUSE_CURSOR_EW_RESIZE,
    GUI_MOUSE_CURSOR_NS_RESIZE,
    GUI_MOUSE_CURSOR_NESW_RESIZE,
    GUI_MOUSE_CURSOR_NWSE_RESIZE,
    GUI_MOUSE_CURSOR_ZOOM_IN,
    GUI_MOUSE_CURSOR_ZOOM_OUT,
} gui_mouse_cursor_t;

/** @} */
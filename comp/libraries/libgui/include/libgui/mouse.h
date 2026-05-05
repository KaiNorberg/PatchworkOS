#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
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
    GUI_MOUSE_BUTTON_LEFT = (1 << 1),
    GUI_MOUSE_BUTTON_RIGHT = (1 << 2),
    GUI_MOUSE_BUTTON_MIDDLE = (1 << 3),
} gui_mouse_buttons_t;

/** @} */
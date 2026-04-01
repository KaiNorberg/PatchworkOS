#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgui/button.h>
#include <stdbool.h>

typedef struct gui_layout gui_layout_t;

/**
 * @brief Keyboard definitions
 * @defgroup comp_libgui_key Keyboard
 * @ingroup comp
 *
 * @{
 */

/**
 * @brief Widget keyboard modifiers.
 * @enum gui_key_mods_t
 */
typedef enum
{
    GUI_KEY_MOD_NONE = 0,
    GUI_KEY_MOD_SHIFT = (1 << 0),
    GUI_KEY_MOD_CTRL = (1 << 1),
    GUI_KEY_MOD_ALT = (1 << 2),
    GUI_KEY_MOD_SUPER = (1 << 3),
    GUI_KEY_MOD_CAPS_LOCK = (1 << 4),
    GUI_KEY_MOD_NUM_LOCK = (1 << 5),
    GUI_KEY_MOD_SCROLL_LOCK = (1 << 6),
} gui_key_mods_t;

/**
 * @brief Widget key states.
 * @enum gui_key_state_t
 */
typedef enum
{
    GUI_KEY_STATE_PRESSED,
    GUI_KEY_STATE_RELEASED,
    GUI_KEY_STATE_HELD,
} gui_key_state_t;

/** @} */
#pragma once

#include <libc/status.h>
#include <libgfx/gfx.h>

typedef struct gui_widget gui_widget_t;
typedef uint32_t gui_widget_id_t;

/**
 * @brief Button Widget
 * @defgroup comp_libgui_button Button
 * @ingroup comp_libgui
 *
 * @{
 */

/**
 * @brief Button style enum.
 * @enum gui_button_style_t
 */
typedef enum gui_button_style
{
    GUI_BUTTON_STYLE_NORMAL,
    GUI_BUTTON_STYLE_FLAT,
    GUI_BUTTON_STYLE_TRANSPARENT,
} gui_button_style_t;

/**
 * @brief Create a new button widget.
 *
 * @param parent The parent widget.
 * @param id The ID of the widget.
 * @param bounds The bounds of the widget.
 * @param style The style of the button.
 * @param out Output pointer for the created button.
 * @return An appropriate status value.
 */
status_t gui_button_new(gui_widget_t* parent, gui_widget_id_t id, gfx_rect_t bounds, gui_button_style_t style,
    gui_widget_t** out);

/**
 * @brief Set the style of a button widget.
 *
 * @param button The button widget.
 * @param style The style to set.
 * @return An appropriate status value.
 */
status_t gui_button_set_style(gui_widget_t* button, gui_button_style_t style);

/** @} */
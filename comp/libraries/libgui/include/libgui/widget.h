#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgui/cursor.h>
#include <libgui/mouse.h>
#include <stdbool.h>

typedef struct gui_layout gui_layout_t;

/**
 * @brief Widgets
 * @defgroup comp_libgui_widget Widgets
 * @ingroup comp
 *
 * A widget is a set of states where any number of said states may by active or inactive, each state consists of a set
 * of properties, with each later defined, active state overiding properties within the previously defined active state.
 *
 * Widgets are arranged in a tree structure, where each widget has a parent and zero or more children.
 *
 * ## States
 *
 * A state is a set of properties, which each property being a key-value pair.
 *
 * All widgets have a "default" state, which will always be considered the first defined state. As such, any other
 * active state will override its properties.
 *
 * This concept that later defined states override previous ones is the core mechanism for widget styling and behavior
 * and also applies to properties, if the same property is defined multiple times the last one will take precedence.
 *
 * ### Examples
 *
 * Say we have three states each with its own background color, defined in the order below:
 *
 * - **Default**: Red
 * - **Hovered**: Blue
 * - **Disabled**: Green
 *
 * In this example, the widget will be Red by default, once hovered it will turn Blue and if disabled it will always
 * turn Green, even if it is also hovered as that Disabled state is declared last.
 *
 * If we were to change the order such that Disabled is declared before Hovered, then a disabled widget that is hovered
 * would turn Blue.
 *
 * @see comp_libgui_properties for more information on properties.
 *
 * @{
 */

/**
 * @brief Hidden widget structure.
 * @struct gui_widget_t
 */
typedef struct gui_widget gui_widget_t;

/**
 * @brief Create a new widget.
 *
 * @param parent The parent widget, can be `NULL`.
 * @param out Output pointer for the created widget, can be `NULL`.
 * @return An appropriate status value.
 */
status_t gui_widget_new(gui_widget_t* parent, gui_widget_t** out);

/**
 * @brief Free a widget and all its children.
 *
 * @param widget The widget to free.
 */
void gui_widget_free(gui_widget_t* widget);

/**
 * @brief Invalidate a specific area of a widget, forcing it to be redrawn on the next tick.
 *
 * @param widget The widget to invalidate.
 * @param area The area to invalidate, relative to the widget's origin.
 */
void gui_widget_invalidate(gui_widget_t* widget, gfx_rect_t area);

/**
 * @brief Get the parent of a widget.
 *
 * @param widget The widget.
 * @return The parent widget, or `NULL` if it has no parent.
 */
gui_widget_t* gui_widget_get_parent(gui_widget_t* widget);

/** @} */
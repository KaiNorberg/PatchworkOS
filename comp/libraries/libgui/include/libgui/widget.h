#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgui/class.h>
#include <libgui/cursor.h>
#include <libgui/event.h>
#include <libgui/mouse.h>
#include <stdbool.h>

typedef struct gui_layout gui_layout_t;

/**
 * @brief Widgets
 * @defgroup comp_libgui_widget Widgets
 * @ingroup comp
 *
 * @{
 */

/**
 * @brief Hidden widget structure.
 * @struct gui_widget_t
 */
typedef struct gui_widget gui_widget_t;

/**
 * @brief Widget ID type.
 *
 * Primarily used to specify the source of certain events.
 */
typedef uint32_t gui_widget_id_t;

/**
 * @brief Create a new generic widget.
 *
 * The created widget will be hidden by default, use `gui_widget_show()` to make it visible.
 *
 * @param cls The class of the widget to create.
 * @param parent The parent widget, can be `NULL`.
 * @param id The ID of the widget, used to report the source of events.
 * @param bounds The bounds of the widget relative to its parent.
 * @param userdata Initial userdata for the widget, can be `NULL`.
 * @param out Output pointer for the created widget, can be `NULL`.
 * @return An appropriate status value.
 */
status_t gui_widget_new(gui_class_t* cls, gui_widget_t* parent, gui_widget_id_t id, gfx_rect_t bounds, void* userdata,
    gui_widget_t** out);

/**
 * @brief Free a widget and all its children.
 *
 * @param widget The widget to free.
 */
void gui_widget_free(gui_widget_t* widget);

/**
 * @brief Show a widget, making it visible.
 *
 * @param widget The widget to show.
 */
void gui_widget_show(gui_widget_t* widget);

/**
 * @brief Hide a widget, making it invisible.
 *
 * @param widget The widget to hide.
 */
void gui_widget_hide(gui_widget_t* widget);

/**
 * @brief Invalidate a specific area of a widget, forcing it to be redrawn on the next tick.
 *
 * @param widget The widget to invalidate.
 * @param area The area to invalidate, relative to the widget's origin.
 */
void gui_widget_invalidate(gui_widget_t* widget, gfx_rect_t area);

/**
 * @brief Force a layout recalculation starting from this widget, potentially invalidating some area.
 *
 * @param widget The widget.
 */
void gui_widget_invalidate_layout(gui_widget_t* widget);

/**
 * @brief Get the bounds of a widget relative to its parent
 *
 * @param widget The widget.
 * @return The bounds of the widget.
 */
gfx_rect_t gui_widget_get_bounds(gui_widget_t* widget);

/**
 * @brief Get the bounds of a widget relative to the root GUI instance.
 *
 * @param widget The widget.
 * @return The absolute bounds of the widget.
 */
gfx_rect_t gui_widget_get_ancestor_bounds(gui_widget_t* widget);

/**
 * @brief Get the bounds of a widget relative to its own origin (0,0).
 *
 * @param widget The widget.
 * @return The local bounds of the widget.
 */
gfx_rect_t gui_widget_get_local_bounds(gui_widget_t* widget);

/**
 * @brief Set the bounds of a widget.
 *
 * @param widget The widget.
 * @param bounds The new bounds.
 * @return An appropriate status value.
 */
status_t gui_widget_set_bounds(gui_widget_t* widget, gfx_rect_t bounds);

/**
 * @brief Get the parent of a widget.
 *
 * @param widget The widget.
 * @return The parent widget, or `NULL` if it has no parent.
 */
gui_widget_t* gui_widget_get_parent(gui_widget_t* widget);

/**
 * @brief Set the parent of a widget.
 *
 * @param widget The widget.
 * @param parent The new parent widget, or `NULL` to detach.
 */
void gui_widget_set_parent(gui_widget_t* widget, gui_widget_t* parent);

/**
 * @brief Get the ID of a widget.
 *
 * @param widget The widget.
 * @return The ID of the widget.
 */
gui_widget_id_t gui_widget_get_id(gui_widget_t* widget);

/**
 * @brief Set the ID of a widget.
 *
 * @param widget The widget.
 * @param id The new ID.
 */
void gui_widget_set_id(gui_widget_t* widget, gui_widget_id_t id);

/**
 * @brief Get the cursor state being used while the mouse is over the widget.
 *
 * @param widget The widget.
 * @return The cursor type.
 */
gui_cursor_t gui_widget_get_cursor(gui_widget_t* widget);

/**
 * @brief Set the cursor state to be used while the mouse is over the widget.
 *
 * @param widget The widget.
 * @param cursor The new cursor type.
 */
void gui_widget_set_cursor(gui_widget_t* widget, gui_cursor_t cursor);

/**
 * @brief Get the custom user data associated with a widget.
 *
 * @param widget The widget.
 * @return The user data pointer.
 */
void* gui_widget_get_userdata(gui_widget_t* widget);

/**
 * @brief Set custom user data for a widget.
 *
 * @param widget The widget.
 * @param userdata The user data pointer.
 */
void gui_widget_set_userdata(gui_widget_t* widget, void* userdata);

/**
 * @brief Check if a widget is visible.
 *
 * @param widget The widget.
 * @return `true` if the widget is visible, `false` otherwise.
 */
bool gui_widget_is_visible(gui_widget_t* widget);

/**
 * @brief Check if a widget is currently hovered by the mouse.
 *
 * @param widget The widget.
 * @return `true` if the widget is hovered, `false` otherwise.
 */
bool gui_widget_is_hovered(gui_widget_t* widget);

/**
 * @brief Check if a widget currently has keyboard focus.
 *
 * @param widget The widget.
 * @return `true` if the widget is focused, `false` otherwise.
 */
bool gui_widget_is_focused(gui_widget_t* widget);

/**
 * @brief Emit an event to a widget.
 *
 * @param widget The widget to emit the event to.
 * @param event The event to emit.
 * @return An appropriate status value.
 */
status_t gui_widget_emit_event(gui_widget_t* widget, gui_event_t* event);

/** @} */
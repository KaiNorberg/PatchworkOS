#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgui/format.h>
#include <libgui/key.h>
#include <libgui/mouse.h>
#include <libgui/widget.h>
#include <stdbool.h>

/**
 * @brief Generic GUI widget library
 * @defgroup comp_libgui libgui
 * @ingroup comp
 *
 * The gui library provides a generic widget infrastructure for building graphical user interfaces.
 *
 * @{
 */

/**
 * @brief Hidden GUI structure.
 * @struct gui_t
 */
typedef struct gui gui_t;

/**
 * @brief Create a new GUI instance.
 *
 * @param screen The graphics context to use as the GUIs frontbuffer, caller is responsible for memory.
 * @param out Output pointer for the created GUI instance.
 * @return An appropriate status value.
 */
status_t gui_new(gfx_t* screen, gui_t** out);

/**
 * @brief Free a GUI instance.
 *
 * @param gui The GUI instance to free.
 */
void gui_free(gui_t* gui);

/**
 * @brief Get the root widget of the GUI.
 *
 * @param gui The GUI instance.
 * @return The root widget.
 */
gui_widget_t* gui_get_root(gui_t* gui);

/**
 * @brief Get the widget currently being hovered by the mouse.
 *
 * @param gui The GUI instance.
 * @return The hovered widget, or `NULL` if no widget is hovered.
 */
gui_widget_t* gui_get_hovered(gui_t* gui);

/**
 * @brief Get the widget currently holding keyboard focus.
 *
 * @param gui The GUI instance.
 * @return The focused widget, or `NULL` if no widget has focus.
 */
gui_widget_t* gui_get_focused(gui_t* gui);

/**
 * @brief Get the dirty area of the GUI.
 *
 * @param gui The GUI instance.
 * @param out Output pointer for the dirty region.
 */
void gui_get_dirty(gui_t* gui, gfx_region_t* out);

/**
 * @brief Set the background color of the GUI.
 *
 * @param gui The GUI instance.
 * @param color The background color.
 */
void gui_set_background(gui_t* gui, gfx_pixel_t color);

/**
 * @brief Get the background color of the GUI.
 *
 * @param gui The GUI instance.
 * @return The background color.
 */
gfx_pixel_t gui_get_background(gui_t* gui);

/**
 * @brief Invalidate a specific area of the GUI, forcing it to be redrawn on the next tick.
 *
 * @param gui The GUI instance.
 * @param area The area to invalidate, relative to the screen.
 */
void gui_invalidate(gui_t* gui, gfx_rect_t area);

/**
 * @brief Get the time until the next scheduled timeout.
 *
 * @param gui The GUI instance.
 * @param now The current time in clock ticks.
 * @return The number of clock ticks until the next timeout.
 */
clock_t gui_next_timeout(gui_t* gui, clock_t now);

/**
 * @brief Update the GUI.
 *
 * @param gui The GUI instance.
 * @param delta The time since the last tick in clock ticks.
 * @return An appropriate status value.
 */
status_t gui_tick(gui_t* gui, clock_t delta);

/**
 * @brief Process mouse input.
 *
 * @param gui The GUI instance.
 * @param x The absolute x-coordinate of the mouse.
 * @param y The absolute y-coordinate of the mouse.
 * @param z The scroll wheel delta.
 * @param pressed The buttons currently pressed.
 * @param released The buttons released since the last input.
 * @return An appropriate status value.
 */
status_t gui_input_mouse(gui_t* gui, int32_t x, int32_t y, int32_t z, gui_mouse_buttons_t pressed,
    gui_mouse_buttons_t released);

/**
 * @brief Process keyboard input.
 *
 * @param gui The GUI instance.
 * @param key The key code.
 * @param state The state of the key (pressed/released).
 * @param modifiers The active key modifiers.
 * @return An appropriate status value.
 */
status_t gui_input_key(gui_t* gui, keycode_t key, gui_key_state_t state, gui_key_mods_t modifiers);

/** @} */
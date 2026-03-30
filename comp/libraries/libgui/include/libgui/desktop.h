#pragma once

#include <libgfx/gfx.h>
#include <libc/status.h>

typedef struct gui_widget gui_widget_t;

/**
 * @brief Desktop Widget
 * @defgroup comp_libgui_desktop Desktop
 * @ingroup comp_libgui
 *
 * @{
 */

/**
 * @brief Create a new desktop widget.
 *
 * @param bounds The bounds of the desktop.
 * @param screen The pixel buffer to render to.
 * @param out Output pointer for the created desktop.
 * @return An appropriate status value.
 */
status_t gui_desktop_new(gfx_rect_t bounds, gfx_pixel_t* screen, gui_widget_t** out);

/** @} */
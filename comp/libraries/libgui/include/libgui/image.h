#pragma once

#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgfx/rect.h>

typedef struct gui_widget gui_widget_t;
typedef uint32_t gui_widget_id_t;

/**
 * @brief Image Widget and Data
 * @defgroup comp_libgui_image Image
 * @ingroup comp_libgui
 *
 * @{
 */

/**
 * @brief Image scaling modes.
 */
typedef enum
{
    GUI_IMAGE_SCALE_STRETCH,
    GUI_IMAGE_SCALE_CENTER,
    GUI_IMAGE_SCALE_FIT,
    GUI_IMAGE_SCALE_TILE
} gui_image_scale_t;

/**
 * @brief In-memory image data structure.
 */
typedef struct gui_image
{
    uint32_t width;
    uint32_t height;
    gfx_pixel_t* pixels;
    gfx_t gfx;
} gui_image_t;

/**
 * @brief Load a PNG image from the filesystem.
 *
 * @param path The path to the PNG image.
 * @param out Output pointer for the loaded image.
 * @return An appropriate status value.
 */
status_t gui_image_load(const char* path, gui_image_t** out);

/**
 * @brief Free a loaded image's buffer.
 *
 * @param image The image to free.
 */
void gui_image_free(gui_image_t* image);

/**
 * @brief Create an image widget.
 * 
 * @param parent The parent widget.
 * @param id The ID for the new widget.
 * @param bounds The initial bounds of the widget.
 * @param image The image data to display.
 * @param out Output pointer for the created widget.
 * @return An appropriate status value.
 */
status_t gui_image_view_new(gui_widget_t* parent, gui_widget_id_t id, gfx_rect_t bounds, gui_image_t* image, gui_widget_t** out);

/**
 * @brief Set the image to be displayed by the image view widget.
 *
 * @param widget The image view widget.
 * @param image The image data to display.
 */
void gui_image_view_set_image(gui_widget_t* widget, gui_image_t* image);

/**
 * @brief Set the scaling mode for the image view widget.
 *
 * @param widget The image view widget.
 * @param scale The scaling mode to use.
 */
void gui_image_view_set_scale(gui_widget_t* widget, gui_image_scale_t scale);

/** @} */

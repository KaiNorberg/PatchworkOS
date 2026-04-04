#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgui/button.h>
#include <libgui/event.h>
#include <stdbool.h>

typedef struct gui_widget gui_widget_t;

/**
 * @brief Class definitions
 * @defgroup comp_libgui_class Class
 * @ingroup comp
 *
 *
 * @{
 */

/**
 * @brief Widget class structure.
 * @struct gui_class_t
 */
typedef struct gui_class
{
    size_t size; ///< Size of the widget structure for this class.
    /**
     * @brief Initialize the widget.
     *
     * @param widget The widget to initialize.
     * @return An appropriate status value.
     */
    status_t (*init)(gui_widget_t* widget);
    /**
     * @brief Deinitialize the widget.
     *
     * @param widget The widget to deinitialize.
     */
    void (*deinit)(gui_widget_t* widget);
    /**
     * @brief Calculate the desired size of the widget based on available space.
     *
     * @param widget The widget to measure.
     * @param availableWidth The maximum available width, or `SIZE_MAX` for unconstrained.
     * @param availableHeight The maximum available height, or `SIZE_MAX` for unconstrained.
     * @return An appropriate status value.
     */
    status_t (*measure)(gui_widget_t* widget, size_t availableWidth, size_t availableHeight);
    /**
     * @brief Draw the widget.
     *
     * @param widget The widget to draw.
     * @param gfx The graphics context to draw to, in widget-local coordinates.
     * @return An appropriate status value.
     */
    status_t (*draw)(gui_widget_t* widget, gfx_t* gfx);
    /**
     * @brief Handle an event for the widget.
     *
     * @param widget The widget.
     * @param event The event to handle.
     * @return An appropriate status value.
     */
    status_t (*procedure)(gui_widget_t* widget, gui_event_t* event);
} gui_class_t;

/** @} */
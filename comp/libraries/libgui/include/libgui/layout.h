#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <stdbool.h>

typedef struct gui_layout gui_layout_t;

/**
 * @brief Layout definitions
 * @defgroup comp_libgui_layout Layout
 * @ingroup comp
 *
 * @{
 */

/**
 * @brief Widget layout structure.
 * @struct gui_layout_t
 *
 * Provides policy for how a parent should arrange its children.
 */
typedef struct gui_layout
{
    /**
     * @brief Arrange the children of a widget.
     *
     * @param widget The parent widget whose children should be arranged.
     * @return An appropriate status value.
     */
    status_t (*arrange)(gui_layout_t* layout, gui_widget_t* widget);
    void* data; ///< Optional private data.
} gui_layout_t;

/** @} */
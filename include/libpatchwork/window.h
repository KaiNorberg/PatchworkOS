#ifndef PATCHWORK_WIN_H
#define PATCHWORK_WIN_H 1

#include "display.h"
#include "pixel.h"
#include "procedure.h"
#include "surface.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Window.
 * @defgroup libpatchwork_window Window
 * @ingroup libpatchwork
 *
 * A window represents a rectangular area on the screen that can display content and receive user input, this includes
 * panels, cursors, wallpapers and normal application windows. It can be considered to be the client side implementation
 * of the Desktop Window Managers surfaces.
 *
 * If `WINDOW_DECO` flag is set, the window will have decorations (titlebar, close/minimize buttons, etc) which will
 * serve as the root element with the client element as a child. This client element is then what the application will
 * draw to and receive events from.
 *
 * The window system is NOT thread safe, it is the responsibility of the application to ensure that windows are only
 * accessed from a single thread at a time.
 *
 * @{
 */

/**
 * @brief Opaque window structure.
 * @struct window_t
 */
typedef struct window window_t;

/**
 * @brief Window flags.
 * @enum window_flags_t
 */
typedef enum window_flags
{
    WINDOW_NONE = 0,
    WINDOW_DECO = 1 << 0,       ///< Enable decorations (titlebar, close/minimize buttons, etc).
    WINDOW_RESIZABLE = 1 << 1,  ///< Allows `window_move()` to resize the window. TODO: Implement resize handles.
    WINDOW_NO_CONTROLS = 1 << 2 ///< Disable controls (close/minimize buttons), only applies if `WINDOW_DECO` is set.
} window_flags_t;

/**
 * @brief Allocate and initialize a new window.
 *
 * @param disp The connection to the DWM.
 * @param name The name of the window.
 * @param rect The rectangle defining the position and size of the window.
 * @param type The type of surface to create, (e.g., panels, cursors, wallpapers, normal windows, etc).
 * @param flags The window flags.
 * @param procedure The procedure for the window's client element.
 * @param private Private data to associate with the window's client element.
 * @return On success, the new window. On failure, returns `NULL` and sets `errno`.
 */
window_t* window_new(display_t* disp, const char* name, const rect_t* rect, surface_type_t type, window_flags_t flags,
    procedure_t procedure, void* private);

/**
 * @brief Free a window.
 *
 * @param win The window to free.
 */
void window_free(window_t* win);

/**
 * @brief Get the window's rectangle in screen coordinates.
 *
 * Equivalent to `RECT_INIT_DIM(x, y, width, height)`.
 *
 * @param win The window.
 * @return The window's rectangle.
 */
rect_t window_get_rect(window_t* win);

/**
 * @brief Get the window's rectangle in local coordinates.
 *
 * Equivalent to `RECT_INIT_DIM(0, 0, width, height)`.
 *
 * @param win The window.
 * @return The window's local rectangle.
 */
rect_t window_content_rect(window_t* win);

/**
 * @brief Get the display associated with the window.
 *
 * @param win The window.
 * @return The display.
 */
display_t* window_get_display(window_t* win);

/**
 * @brief Get the surface ID of the window.
 *
 * @param win The window.
 * @return The surface ID or `SURFACE_ID_NONE` if `win` is `NULL`.
 */
surface_id_t window_get_id(window_t* win);

/**
 * @brief Get the surface type of the window.
 *
 * @param win The window.
 * @return The surface type or `SURFACE_NONE` if `win` is `NULL`.
 */
surface_type_t window_get_type(window_t* win);

/**
 * @brief Get the client element of the window.
 *
 * The client element is the window element that applications should draw to and receive events from, if the window has
 * decorations this will be the child of the deco element, otherwise it will be the root element.
 *
 * @param win The window.
 * @return The client element.
 */
element_t* window_get_client_element(window_t* win);

/**
 * @brief Move and/or resize the window.
 *
 * @param win The window.
 * @param rect The new screen rectangle for the window.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t window_move(window_t* win, const rect_t* rect);

/**
 * @brief Set the window timer.
 *
 * When the timer fires an event of type `EVENT_TIMER` will be sent to the window's procedure.
 *
 * @param win The window.
 * @param flags The timer flags.
 * @param timeout The timer timeout in clock ticks, or `CLOCKS_NEVER` to disable the timer. Setting a new timer will
 * overide the previous timer if one is set.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t window_set_timer(window_t* win, timer_flags_t flags, clock_t timeout);

/**
 * @brief Invalidate a rectangle of the window.
 *
 * This is used to notify the DWM of the change not to redraw the specified rectangle.
 *
 * The changes will be flushed to the DWM when `window_invalidate_flush()` is called.
 *
 * @param win The window.
 * @param rect The rectangle to invalidate, in local coordinates.
 */
void window_invalidate(window_t* win, const rect_t* rect);

/**
 * @brief Flush invalidated rectangles to the DWM.
 *
 * This will send the invalidated rectangle to the DWM and clear the invalid rectangle.
 *
 * @param win The window.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t window_invalidate_flush(window_t* win);

/**
 * @brief Dispatch an event to the window's elements.
 *
 * Most events will be sent to the root element, which will then propagate the event to its children.
 *
 * Some events will be handled specially, for example `LEVENT_FORCE_ACTION` will be sent directly to the specified
 * element.
 *
 * @param win The window.
 * @param event The event to dispatch.
 * @return The result of the window procedure.
 */
uint64_t window_dispatch(window_t* win, const event_t* event);

/**
 * @brief Set the focus to the window.
 *
 * Causes the window to be moved to the front and to, for example, receive keyboard input.
 *
 * @param win The window.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t window_set_focus(window_t* win);

/**
 * @brief Set the visibility of the window.
 *
 * Windows are invisible by default, so they must be made visible after creation to be seen.
 *
 * @param win The window.
 * @param isVisible Whether the window should be visible.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t window_set_visible(window_t* win, bool isVisible);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

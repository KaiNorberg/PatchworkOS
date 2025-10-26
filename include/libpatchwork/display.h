#ifndef PATCHWORK_DISPLAY_H
#define PATCHWORK_DISPLAY_H 1

#include "cmd.h"
#include "event.h"
#include "rect.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Display connection.
 * @defgroup libpatchwork_display Display Connection
 * @ingroup libpatchwork
 *
 * A display represents a connection to the Desktop Window Manager (DWM).
 *
 * The display system is NOT thread safe, it is the responsibility of the application to ensure that windows are only
 * accessed from a single thread at a time.
 * @{
 */

/**
 * @brief Opaque display structure.
 * @struct display_t
 */
typedef struct display display_t;

/**
 * @brief Create a new display connection.
 *
 * @return On success, a The display connection. On failure, `NULL` and `errno` is set.
 */
display_t* display_new(void);

/**
 * @brief Free a display connection.
 *
 * @param disp The display connection.
 */
void display_free(display_t* disp);

/**
 * @brief Get the rectangle of a screen.
 *
 * @param disp The display connection.
 * @param index Index of the screen to query, only `0` is supported currently.
 * @return The rectangle of the screen, or a zero-area rectangle on failure.
 */
rect_t display_screen_rect(display_t* disp, uint64_t index);

/**
 * @brief Get the data file descriptor of the display connection.
 *
 * This file descriptor can be used with `poll()` to wait for events from the DWM and other files at the same time.
 *
 * @param disp The display connection.
 * @return On success, the data file descriptor. On failure, returns `ERR` and sets `errno`.
 */
fd_t display_data_fd(display_t* disp);

/**
 * @brief Check if the display connection is still connected.
 *
 * @param disp The display connection.
 * @return `true` if connected, `false` otherwise.
 */
bool display_is_connected(display_t* disp);

/**
 * @brief Disconnect the display connection.
 *
 * After calling this function, the display connection will be marked as disconnected and no further commands or
 * events will be processed.
 *
 * Will not free the display connection, use `display_free()` for that.
 *
 * @param disp The display connection.
 */
void display_disconnect(display_t* disp);

/**
 * @brief Allocate a section of the displays command buffer.
 *
 * The display batches commands together in its command buffer, where each command is prefixed with a `cmd_header_t` and
 * has a variable size.
 *
 * @param disp The display connection.
 * @param type Type of command to allocate.
 * @param size Size of the command data to allocate.
 * @return Pointer to the allocated command data, or `NULL` on failure and sets `errno`.
 */
void* display_cmd_alloc(display_t* disp, cmd_type_t type, uint64_t size);

/**
 * @brief Flush the display's command buffer.
 *
 * This will send all queued commands to the DWM.
 *
 * @param disp The display connection.
 */
void display_cmds_flush(display_t* disp);

/**
 * @brief Retrieve the next event from the display connection.
 *
 * @param disp The display connection.
 * @param event Output pointer to store the retrieved event.
 * @param timeout Maximum time to wait for an event, if `CLOCKS_NEVER` will wait indefinitely.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t display_next_event(display_t* disp, event_t* event, clock_t timeout);

/**
 * @brief Push an event to the display's internal event queue.
 *
 * This will not send the event to the DWM, instead it will be stored in the display's internal event queue and can be
 * retrieved using `display_next_event()`.
 *
 * If the event queue is full, the event at the front of the queue will be discarded to make room for the new event.
 *
 * @param disp The display connection.
 * @param target Target surface ID for the event.
 * @param type Type of event.
 * @param data Pointer to the event data, can be `NULL` if `size` is `0`.
 * @param size Size of the event data, must be less than `EVENT_MAX_DATA`.
 */
void display_events_push(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size);

/**
 * @brief Wait for the display to receive an event of the expected type.
 *
 * This function will block until an event of the expected type is received.
 *
 * Any other events received while waiting will be pushed back to the display's internal event queue.
 *
 * @param disp The display connection.
 * @param event Output pointer to store the retrieved event.
 * @param expected The expected event type to wait for.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t display_wait_for_event(display_t* disp, event_t* event, event_type_t expected);

/**
 * @brief Emit an event to a target surface.
 *
 * This function will construct an event and dispatch it to the target surface.
 *
 * @param disp The display connection.
 * @param target Target surface ID for the event, if `SURFACE_ID_NONE` the event is sent to all surfaces.
 * @param type Type of event.
 * @param data Pointer to the event data, can be `NULL` if `size` is `0`.
 * @param size Size of the event data, must be less than `EVENT_MAX_DATA`.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t display_emit(display_t* disp, surface_id_t target, event_type_t type, void* data, uint64_t size);

/**
 * @brief Dispatch an event to the appropriate surface.
 *
 * @param disp The display connection.
 * @param event The event to dispatch.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t display_dispatch(display_t* disp, const event_t* event);

/**
 * @brief Subscribe to events of a specific type.
 *
 * Should only be used for events sent by the DWM.
 *
 * @param disp The display connection.
 * @param type The event type to subscribe to.
 */
uint64_t display_subscribe(display_t* disp, event_type_t type);

/**
 * @brief Unsubscribe from events of a specific type.
 *
 * Should only be used for events sent by the DWM.
 *
 * @param disp The display connection.
 * @param type The event type to unsubscribe from.
 */
uint64_t display_unsubscribe(display_t* disp, event_type_t type);

/**
 * @brief Get information about a surface.
 *
 * Uses a `CMD_SURFACE_REPORT` command to request information about the specified surface from the DWM.
 *
 * @param disp The display connection.
 * @param id The surface ID to query.
 * @param info Output pointer to store the surface information.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t display_get_surface_info(display_t* disp, surface_id_t id, surface_info_t* info);

/**
 * @brief Set the focus to a surface.
 *
 * Can apply to any surface, not just ones owned by the client.
 *
 * @param disp The display connection.
 * @param id The surface ID to set focus to.
 */
uint64_t display_set_focus(display_t* disp, surface_id_t id);

/**
 * @brief Set the visibility of a surface.
 *
 * Can apply to any surface, not just ones owned by the client.
 *
 * @param disp The display connection.
 * @param id The surface ID to set visibility for.
 * @param isVisible Whether the surface should be visible.
 */
uint64_t display_set_is_visible(display_t* disp, surface_id_t id, bool isVisible);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

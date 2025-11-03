#ifndef PATCHWORK_ELEMENT_H
#define PATCHWORK_ELEMENT_H 1

#include "cmd.h"
#include "drawable.h"
#include "element_id.h"
#include "font.h"
#include "procedure.h"
#include "rect.h"
#include "theme.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief UI Elements.
 * @defgroup libpatchwork_element UI Elements
 * @ingroup libpatchwork
 *
 * A window is made up of a tree of elements, each element is responsible for drawing a part of the window and handling
 * events for that part. Elements can have child elements, which are drawn on top of the parent element.
 *
 * Each element will on creation, copy the current global theme as its own theme, which can then be modified on a
 * per-element basis.
 *
 * @{
 */

/**
 * @brief Element flags type.
 * @typedef element_flags_t
 *
 * We make this a uint64_t instead an an enum to give us more flags for the future.
 */
typedef uint64_t element_flags_t;

#define ELEMENT_NONE 0
#define ELEMENT_TOGGLE (1 << 0)
#define ELEMENT_FLAT (1 << 1)
#define ELEMENT_NO_BEZEL (1 << 2)
#define ELEMENT_NO_OUTLINE (1 << 3)

/**
 * @brief Opaque element structure.
 * @struct element_t
 */
typedef struct element element_t;

/**
 * @brief Element text properties structure.
 * @struct text_props_t
 *
 * To avoid code duplication, we implement a text properties structure that can be used by any element that
 * needs to render text.
 */
typedef struct
{
    align_t xAlign;
    align_t yAlign;
    font_t* font;
} text_props_t;

/**
 * @brief Element image properties structure.
 * @struct image_props_t
 *
 * To avoid code duplication, we implement an image properties structure that can be used by any element that
 * needs to render an image.
 */
typedef struct
{
    align_t xAlign;
    align_t yAlign;
    point_t srcOffset;
} image_props_t;

/**
 * @brief Allocate and initialize a new element.
 *
 * Will send a fake `EVENT_LIB_INIT` event to the element after creation, followed by a real `EVENT_LIB_REDRAW` event.
 *
 * A event being fake just means its sent by directly calling the element procedure, instead of being pushed to the
 * display's event queue.
 *
 * @param parent The parent element.
 * @param id The element ID.
 * @param rect The elements rectangle relative to its parent.
 * @param text The elements text, if the element is for example a button, this will be the button label.
 * @param flags The element flags.
 * @param procedure The element procedure.
 * @param private Pointer to private data for the element.
 * @return On success, a pointer to the new element. On failure, `NULL` and `errno` is set.
 */
element_t* element_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags,
    procedure_t procedure, void* private);

/**
 * @brief Deinitialize and free an element and all its children.
 *
 * Will send a fake `EVENT_LIB_DEINIT` event to the element before freeing it.
 *
 * @param elem The element to free.
 */
void element_free(element_t* elem);

/**
 * @brief Find a child element by its ID.
 *
 * Will search recursively through all child elements.
 *
 * @param elem The element to search from.
 * @param id The element ID to search for.
 * @return A pointer to the found element, or `NULL` if not found.
 */
element_t* element_find(element_t* elem, element_id_t id);

/**
 * @brief Set private data for an element.
 *
 * @param elem The element.
 * @param private Pointer to the private data.
 */
void element_set_private(element_t* elem, void* private);

/**
 * @brief Get private data for an element.
 *
 * @param elem The element.
 * @return Pointer to the private data, or `NULL` if none is set.
 */
void* element_get_private(element_t* elem);

/**
 * @brief Get the ID of an element.
 *
 * @param elem The element.
 * @return The element ID, or `ELEMENT_ID_NONE` if `elem` is `NULL`.
 */
element_id_t element_get_id(element_t* elem);

/**
 * @brief Move an element to a new rectangle in its parent's coordinate space.
 *
 * Will NOT redraw the element, call `element_redraw()` if needed.
 *
 * @param elem The element.
 * @param rect The new rectangle.
 */
void element_move(element_t* elem, const rect_t* rect);

/**
 * @brief Get the rectangle of an element in its parent's coordinate space.
 *
 * Equivalent to `RECT_INIT_DIM(x, y, width, height)`.
 *
 * @param elem The element.
 * @return The element rectangle, or a zero-area rectangle if `elem` is `NULL`.
 */
rect_t element_get_rect(element_t* elem);

/**
 * @brief Get the element's rectangle in local coordinates.
 *
 * Equivalent to `RECT_INIT_DIM(0, 0, width, height)`.
 *
 * @param elem The element.
 * @return The content rectangle, or a zero-area rectangle if `elem` is `NULL`.
 */
rect_t element_get_content_rect(element_t* elem);

/**
 * @brief Get the rectangle of an element in window coordinates.
 *
 * @param elem The element.
 * @return The element rectangle in window coordinates, or a zero-area rectangle if `elem` is `NULL`.
 */
rect_t element_get_window_rect(element_t* elem);

/**
 * @brief Get the top-left point of an element in window coordinates.
 *
 * @param elem The element.
 * @return The top-left point in window coordinates, or (0, 0) if `elem` is `NULL`.
 */
point_t element_get_window_point(element_t* elem);

/**
 * @brief Convert a rectangle from element coordinates to window coordinates.
 *
 * @param elem The element.
 * @param src The source rectangle in element coordinates.
 * @return The rectangle in window coordinates, or a zero-area rectangle if `elem` or `src` is `NULL`.
 */
rect_t element_rect_to_window(element_t* elem, const rect_t* src);

/**
 * @brief Convert a point from element coordinates to window coordinates.
 *
 * @param elem The element.
 * @param src The source point in element coordinates.
 * @return The point in window coordinates, or (0, 0) if `elem` or `src` is `NULL`.
 */
point_t element_point_to_window(element_t* elem, const point_t* src);

/**
 * @brief Convert a rectangle from window coordinates to element coordinates.
 *
 * @param elem The element.
 * @param src The source rectangle in window coordinates.
 * @return The rectangle in element coordinates, or a zero-area rectangle if `elem` or `src` is `NULL`.
 */
rect_t element_window_to_rect(element_t* elem, const rect_t* src);

/**
 * @brief Convert a point from window coordinates to element coordinates.
 *
 * @param elem The element.
 * @param src The source point in window coordinates.
 * @return The point in element coordinates, or (0, 0) if `elem` or `src` is `NULL`.
 */
point_t element_window_to_point(element_t* elem, const point_t* src);

/**
 * @brief Get the flags of an element.
 *
 * @param elem The element.
 * @return The element flags, or `ELEMENT_NONE` if `elem` is `NULL`.
 */
element_flags_t element_get_flags(element_t* elem);

/**
 * @brief Set the flags of an element.
 *
 * @param elem The element.
 * @param flags The new element flags.
 */
void element_set_flags(element_t* elem, element_flags_t flags);

/**
 * @brief Get the text of an element.
 *
 * @param elem The element.
 * @return The element text, or `NULL` if `elem` is `NULL`.
 */
const char* element_get_text(element_t* elem);

/**
 * @brief Set the text of an element.
 *
 * Will NOT redraw the element, call `element_redraw()` if needed.
 *
 * @param elem The element.
 * @param text The new text.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t element_set_text(element_t* elem, const char* text);

/**
 * @brief Get the text properties of an element.
 *
 * The returned pointer can be used to modify the text properties.
 *
 * @param elem The element.
 * @return Pointer to the text properties, or `NULL` if `elem` is `NULL`.
 */
text_props_t* element_get_text_props(element_t* elem);

/**
 * @brief Get the image of an element.
 *
 * @param elem The element.
 * @return Pointer to the image, or `NULL` if `elem` is `NULL` or has no image.
 */
image_t* element_get_image(element_t* elem);

/**
 * @brief Set the image of an element.
 *
 * Will NOT redraw the element, call `element_redraw()` if needed.
 *
 * @param elem The element.
 * @param image Pointer to the new image or `NULL` to remove the image.
 */
void element_set_image(element_t* elem, image_t* image);

/**
 * @brief Get the image properties of an element.
 *
 * The returned pointer can be used to modify the image properties.
 *
 * @param elem The element.
 * @return Pointer to the image properties, or `NULL` if `elem` is `NULL`.
 */
image_props_t* element_get_image_props(element_t* elem);

/**
 * @brief Get the theme of an element.
 *
 * @param elem The element.
 * @return Pointer to the theme, or `NULL` if `elem` is `NULL`.
 */
theme_t* element_get_theme(element_t* elem);

/**
 * @brief Begin drawing to an element.
 *
 * Note that since this will fill the drawable structure with the element's content rectangle, if the element is for
 * example moved or resized the drawable will be invalid.
 *
 * @param elem The element to draw to.
 * @param draw Pointer to the drawable structure to initialize.
 */
void element_draw_begin(element_t* elem, drawable_t* draw);

/**
 * @brief End drawing to an element.
 *
 * This will invalidate the area of the element that was drawn to and send redraw events to any child elements that
 * overlap the invalid area.
 *
 * @param elem The element that was drawn to.
 * @param draw Pointer to the drawable structure that was used for drawing.
 */
void element_draw_end(element_t* elem, drawable_t* draw);

/**
 * @brief Redraw an element.
 *
 * Will push a `EVENT_LIB_REDRAW` event to the display event queue for the element, meaning the redraw event is not
 * processed immediately.
 *
 * @param elem The element to redraw.
 * @param shouldPropagate Whether the redraw event should propagate to child elements.
 */
void element_redraw(element_t* elem, bool shouldPropagate);

/**
 * @brief Force an action on an element.
 *
 * Will push a `EVENT_LIB_FORCE_ACTION` event to the display event queue for the element, meaning the action event is
 * not processed immediately.
 *
 * @param elem The element.
 * @param action The action to force.
 */
void element_force_action(element_t* elem, action_type_t action);

/**
 * @brief Dispatch an event to an element.
 *
 * This will call the element's procedure with the given event after some preprocessing.
 *
 * Most events will also be propagated to child elements by the element's procedure.
 *
 * @param elem The element.
 * @param event The event to dispatch.
 * @return The return value of the element's procedure.
 */
uint64_t element_dispatch(element_t* elem, const event_t* event);

/**
 * @brief Emit an event to an element.
 *
 * This function will construct an event and dispatch it to the element.
 *
 * @param elem The element.
 * @param type The event type.
 * @param data Pointer to the event data, can be `NULL` if `size` is `0`.
 * @param size The size of the event data.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t element_emit(element_t* elem, event_type_t type, const void* data, uint64_t size);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

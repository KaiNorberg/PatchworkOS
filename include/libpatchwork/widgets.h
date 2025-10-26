#ifndef PATCHWORK_WIDGETS_H
#define PATCHWORK_WIDGETS_H 1

#include <stdint.h>

#include "drawable.h"
#include "element.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Widget element wrappers.
 * @defgroup libpatchwork_widgets Widgets
 * @ingroup libpatchwork
 *
 * These functions create common widget elements such as buttons and labels by creating elements with predefined
 * procedures and private data.
 *
 * @{
 */

/**
 * @brief Create a new button element.
 *
 * @param parent The parent element.
 * @param id The element ID.
 * @param rect The rectangle defining the button's position and size.
 * @param text The button's text.
 * @param flags Element flags.
 * @return On success, a pointer to the newly created button element. On failure, `NULL` and `errno` is set.
 */
element_t* button_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags);

/**
 * @brief Create a new label element.
 *
 * @param parent The parent element.
 * @param id The element ID.
 * @param rect The rectangle defining the label's position and size.
 * @param text The label's text.
 * @param flags Element flags.
 * @return On success, a pointer to the newly created label element. On failure, `NULL` and `errno` is set.
 */
element_t* label_new(element_t* parent, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

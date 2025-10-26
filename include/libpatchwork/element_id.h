#ifndef PATCHWORK_ELEMENT_ID_H
#define PATCHWORK_ELEMENT_ID_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @addtogroup libpatchwork_element
 * @{
 */

/**
 * @brief Element identifier type.
 * @typedef element_id_t
 *
 * Used to send events to specific elements and to know which element sent an event, for example to know which button
 * was pressed in a `LEVENT_ACTION` event.
 */
typedef uint64_t element_id_t;

/**
 * @brief Element ID indicating no element.
 */
#define ELEMENT_ID_NONE UINT64_MAX

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

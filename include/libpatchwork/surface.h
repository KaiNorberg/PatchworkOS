#ifndef PATCHWORK_SURFACE_H
#define PATCHWORK_SURFACE_H 1

#include "rect.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/io.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Desktop Window Manager Surface.
 * @defgroup libpatchwork_surface Surface
 * @ingroup libpatchwork
 *
 * A surface represents a rectangular area on the screen that can display content and receive user input, this includes
 * panels, cursors, wallpapers and normal application windows. It can be considered to be the server side implementation
 * of the client side windows.
 *
 * @{
 */

/**
 * @brief Surface types.
 * @enum surface_type_t
 */
typedef enum
{
    SURFACE_NONE = 0,
    SURFACE_WINDOW,
    SURFACE_PANEL,
    SURFACE_CURSOR,
    SURFACE_WALL,
    SURFACE_FULLSCREEN,
    SURFACE_TYPE_AMOUNT
} surface_type_t;

/**
 * @brief Surface flags.
 * @enum surface_flags_t
 */
typedef enum
{
    SURFACE_VISIBLE = 1 << 0,
    SURFACE_FOCUSED = 1 << 1,
} surface_flags_t;

typedef uint64_t surface_id_t;
#define SURFACE_ID_NONE (UINT64_MAX)

typedef struct
{
    surface_type_t type;
    surface_id_t id;
    rect_t rect;
    surface_flags_t flags;
    char name[MAX_NAME];
    uint8_t reserved[35];
} surface_info_t;

#ifdef static_assert
static_assert(sizeof(surface_info_t) == 104, "invalid surface_info_t size");
#endif

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

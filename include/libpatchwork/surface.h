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

typedef uint64_t surface_id_t;
#define SURFACE_ID_NONE (UINT64_MAX)

typedef struct
{
    surface_type_t type;
    surface_id_t id;
    rect_t rect;
    bool isVisible;
    bool isFocused;
    char name[MAX_NAME];
    uint8_t reserved[35];
} surface_info_t;

#ifdef static_assert
static_assert(sizeof(surface_info_t) == 104, "invalid surface_info_t size");
#endif

#if defined(__cplusplus)
}
#endif

#endif

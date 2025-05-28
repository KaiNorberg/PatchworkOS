#ifndef PATCHWORK_SURFACE_H
#define PATCHWORK_SURFACE_H 1

#include "rect.h"

#include <stdint.h>
#include <stdbool.h>
#include <sys/io.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
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
    bool visible;
    bool focused;
    char name[MAX_NAME];
} surface_info_t;

#if defined(__cplusplus)
}
#endif

#endif

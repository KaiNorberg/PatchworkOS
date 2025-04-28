#ifndef _DWM_SURFACE_H
#define _DWM_SURFACE_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    SURFACE_WINDOW = 0,
    SURFACE_FULLSCREEN = 1,
    SURFACE_PANEL = 2,
    SURFACE_CURSOR = 3,
    SURFACE_WALL = 4,
    SURFACE_TYPE_AMOUNT = 5
} surface_type_t;

typedef uint64_t surface_id_t;
#define SURFACE_ID_ROOT (UINT64_MAX)

#if defined(__cplusplus)
}
#endif

#endif

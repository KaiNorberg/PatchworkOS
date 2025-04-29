#ifndef _DWM_SURFACE_H
#define _DWM_SURFACE_H 1

#include <stdint.h>

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
    SURFACE_TYPE_AMOUNT
} surface_type_t;

typedef uint64_t surface_id_t;
#define SURFACE_ID_NONE (UINT64_MAX)

#if defined(__cplusplus)
}
#endif

#endif

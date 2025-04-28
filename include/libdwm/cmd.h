#ifndef _DWM_CMD_H
#define _DWM_CMD_H 1

#include "surface.h"
#include "rect.h"
#include "pixel.h"

#include <stdint.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    CMD_SCREEN_INFO,
    CMD_SURFACE_NEW,
    CMD_SURFACE_FREE,
    CMD_DRAW_RECT,
    CMD_TOTAL_AMOUNT, // Above this are unimplemented cmds.
    CMD_DRAW_GRADIENT,
    CMD_DRAW_LINE,
    CMD_DRAW_POINT,
    CMD_DRAW_TRIANGLE,
    CMD_DRAW_CIRCLE,
    CMD_DRAW_IMAGE,
    CMD_DRAW_BITMAP
} cmd_type_t;

typedef struct
{
    uint64_t index;
} cmd_screen_info_t;

typedef struct
{
    surface_id_t id;
    surface_id_t parent;
    surface_type_t type;
    rect_t rect;
} cmd_surface_new_t;

typedef struct
{
    surface_id_t target;
} cmd_surface_free_t;

typedef struct
{
    surface_id_t target;
    rect_t rect;
    pixel_t pixel;
} cmd_draw_rect_t;

typedef struct cmd
{
    cmd_type_t type;
    union {
        cmd_screen_info_t screenInfo;
        cmd_surface_new_t surfaceNew;
        cmd_surface_free_t surfaceFree;
        cmd_draw_rect_t drawRect;
        uint8_t padding[64];
    };
} cmd_t;

#define CMD_BUFFER_MAX_CMD (((PAGE_SIZE) / 2) / (sizeof(cmd_t)) - 1)
#define CMD_BUFFER_SIZE(amount) (offsetof(cmd_buffer_t, buffer) + (amount) * sizeof(cmd_t))

typedef struct cmd_buffer
{
    uint64_t amount;
    cmd_t buffer[CMD_BUFFER_MAX_CMD];
} cmd_buffer_t;

#if defined(__cplusplus)
}
#endif

#endif

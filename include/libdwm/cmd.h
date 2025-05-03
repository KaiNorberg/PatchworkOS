#ifndef DWM_CMD_H
#define DWM_CMD_H 1

#include "font_id.h"
#include "pixel.h"
#include "point.h"
#include "rect.h"
#include "surface.h"

#include <stdint.h>
#include <sys/io.h>
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
    CMD_DRAW_EDGE,
    CMD_DRAW_GRADIENT,
    CMD_FONT_NEW,
    CMD_FONT_FREE,
    CMD_FONT_INFO,
    CMD_DRAW_STRING,
    CMD_TYPE_AMOUNT, // Below this are unimplemented cmds.
    CMD_DRAW_LINE,
    CMD_DRAW_POINT,
    CMD_DRAW_TRIANGLE,
    CMD_DRAW_CIRCLE,
    CMD_DRAW_IMAGE,
    CMD_DRAW_BITMAP
} cmd_type_t;

#define CMD_MAGIC 0xDEADC0DE

#define CMD_INIT(cmd, cmdTypeEnum, cmdType) \
    ({ \
        (cmd)->header.magic = CMD_MAGIC; \
        (cmd)->header.type = cmdTypeEnum; \
        (cmd)->header.size = sizeof(cmdType); \
    })

// TODO: Consider way to "disable" the dwm to allow a program to draw directly to the screen via the framebuffers. cmd_enable?
// cmd_disable? Persmissions?

typedef struct cmd_header
{
    uint32_t magic;
    cmd_type_t type;
    uint64_t size;
} cmd_header_t;

typedef struct
{
    cmd_header_t header;
    uint64_t index;
} cmd_screen_info_t;

typedef struct
{
    cmd_header_t header;
    surface_id_t id;
    surface_type_t type;
    rect_t rect;
} cmd_surface_new_t;

typedef struct
{
    cmd_header_t header;
    surface_id_t target;
} cmd_surface_free_t;

typedef struct
{
    cmd_header_t header;
    surface_id_t target;
    rect_t rect;
    pixel_t pixel;
} cmd_draw_rect_t;

typedef struct
{
    cmd_header_t header;
    surface_id_t target;
    rect_t rect;
    uint64_t width;
    pixel_t foreground;
    pixel_t background;
} cmd_draw_edge_t;

typedef enum
{
    GRADIENT_VERTICAL,
    GRADIENT_HORIZONTAL,
    GRADIENT_DIAGONAL
} gradient_type_t;

typedef struct
{
    cmd_header_t header;
    surface_id_t target;
    rect_t rect;
    pixel_t start;
    pixel_t end;
    gradient_type_t type;
    bool addNoise;
} cmd_draw_gradient_t;

typedef struct
{
    cmd_header_t header;
    char name[MAX_NAME];
    uint64_t desiredHeight;
} cmd_font_new_t;

typedef struct
{
    cmd_header_t header;
    font_id_t id;
} cmd_font_free_t;

typedef struct
{
    cmd_header_t header;
    font_id_t id;
} cmd_font_info_t;

typedef struct
{
    cmd_header_t header;
    surface_id_t target;
    font_id_t fontId;
    point_t point;
    pixel_t foreground;
    pixel_t background;
    uint64_t length;
    char string[];
} cmd_draw_string_t;

#define CMD_BUFFER_MAX_DATA (0x1000)

#define CMD_BUFFER_FOR_EACH(buffer, cmd) \
    for (uint8_t *_ptr = (buffer)->data, *_end = (uint8_t*)((uint64_t)(buffer) + (buffer)->size); _ptr < _end; \
        _ptr += ((cmd_header_t*)_ptr)->size) \
        for (cmd = (cmd_header_t*)_ptr; cmd; cmd = NULL)

typedef struct cmd_buffer
{
    uint64_t amount;
    uint64_t size; // The entire used size of the cmd_buffer.
    uint8_t data[CMD_BUFFER_MAX_DATA];
} cmd_buffer_t;

#if defined(__cplusplus)
}
#endif

#endif

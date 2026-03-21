#ifndef PATCHWORK_CMD_H
#define PATCHWORK_CMD_H 1

#include "event.h"
#include "pixel.h"
#include "point.h"
#include "rect.h"
#include "surface.h"

#include <stdint.h>
#include <sys/fs.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define PFAIL (UINT64_MAX)

// Note: Commands will only let you access a surface owned by the client the command is called by unless that command
// has a "bool isGlobal" member and that member is true.

typedef enum
{
    CMD_SCREEN_INFO,
    CMD_SURFACE_NEW,
    CMD_SURFACE_FREE,
    CMD_SURFACE_MOVE,
    CMD_SURFACE_TIMER_SET,
    CMD_SURFACE_INVALIDATE,
    CMD_SURFACE_FOCUS_SET,
    CMD_SURFACE_VISIBLE_SET,
    CMD_SURFACE_REPORT,
    CMD_SUBSCRIBE,
    CMD_UNSUBSCRIBE,
    CMD_TYPE_AMOUNT,
} cmd_type_t;

#define CMD_MAGIC 0xDEADC0DE

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
    surface_type_t type;
    rect_t rect;
    char name[MAX_NAME];
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
} cmd_surface_move_t;

typedef enum
{
    TIMER_NONE = 0,
    TIMER_REPEAT = 1 << 0
} timer_flags_t;

typedef struct
{
    cmd_header_t header;
    surface_id_t target;
    timer_flags_t flags;
    clock_t timeout;
} cmd_surface_timer_set_t;

typedef struct
{
    cmd_header_t header;
    surface_id_t target;
    rect_t invalidRect;
} cmd_surface_invalidate_t;

typedef struct
{
    cmd_header_t header;
    bool isGlobal;
    surface_id_t target;
} cmd_surface_focus_set_t;

typedef struct
{
    cmd_header_t header;
    bool isGlobal;
    surface_id_t target;
    bool isVisible;
} cmd_surface_visible_set_t;

typedef struct
{
    cmd_header_t header;
    bool isGlobal;
    surface_id_t target;
} cmd_surface_report_t;

typedef struct
{
    cmd_header_t header;
    event_type_t event;
} cmd_subscribe_t;

typedef struct
{
    cmd_header_t header;
    event_type_t event;
} cmd_unsubscribe_t;

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

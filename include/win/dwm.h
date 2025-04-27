#ifndef _SYS_DWM_H
#define _SYS_DWM_H 1

#include <stdint.h>
#include <sys/proc.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    DWM_WINDOW = 0,
    DWM_FULLSCREEN = 1,
    DWM_PANEL = 2,
    DWM_CURSOR = 3,
    DWM_WALL = 4,
    DWM_MAX = 4
} dwm_type_t;

#define CMD_BUFFER_MAX_CMD (((PAGE_SIZE) / 2) / (sizeof(cmd_t)) - 1)

typedef enum
{
    CMD_SCREEN_INFO,
    CMD_TOTAL_AMOUNT
} cmd_type_t;

typedef struct
{
    uint64_t index;
} cmd_screen_info_t;

typedef struct cmd
{
    cmd_type_t type;
    union
    {
        cmd_screen_info_t screenInfo;
        uint8_t padding[64];
    };
} cmd_t;

typedef struct cmd_buffer
{
    uint64_t amount;
    cmd_t buffer[CMD_BUFFER_MAX_CMD];
} cmd_buffer_t;

#define CMD_BUFFER_SIZE(amount) (offsetof(cmd_buffer_t, buffer) + (amount) * sizeof(cmd_t))

// Note: Event types are not enums to allow for user defined event types.
typedef uint16_t event_type_t;

#define EVENT_SCREEN_INFO 0

typedef struct
{
    uint64_t width;
    uint64_t height;
} event_screen_info_t;

typedef struct event
{
    event_type_t type;
    uint8_t data[64];
} event_t;

#if defined(__cplusplus)
}
#endif

#endif

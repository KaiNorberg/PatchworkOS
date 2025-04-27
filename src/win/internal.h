typedef struct win win_t;
typedef struct display display_t;
typedef struct widget widget_t;

#define _WIN_INTERNAL
#include <win/win.h>

#include <stdlib.h>
#include <sys/io.h>

#define DISPLAY_MAX_EVENT 32

typedef struct display
{
    fd_t handle;
    char id[MAX_NAME];
    fd_t data;
    cmd_buffer_t cmds;
    struct
    {
        event_t buffer[DISPLAY_MAX_EVENT];
        uint64_t readIndex;
        uint64_t writeIndex;
    } events;
} display_t;

typedef struct win
{
    char name[MAX_NAME];
    point_t pos;
    uint32_t width;
    uint32_t height;
    rect_t clientRect;
} win_t;

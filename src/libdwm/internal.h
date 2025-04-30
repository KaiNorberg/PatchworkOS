#include <libdwm/dwm.h>

#include <sys/io.h>
#include <sys/list.h>

typedef struct element
{
    list_entry_t entry;
    list_t children;
    struct element* parent;
    rect_t rect;
    procedure_t proc;
    window_t* win;
    void* private;
} element_t;

typedef struct window
{
    list_entry_t entry;
    surface_id_t id;
    char name[MAX_NAME];
    rect_t rect;
    surface_type_t type;
    display_t* disp;
    element_t* root;
} window_t;

uint64_t window_dispatch(window_t* win, event_t* event);

#define DISPLAY_MAX_EVENT 32

typedef struct display
{
    fd_t handle;
    char id[MAX_NAME];
    fd_t data;
    bool connected;
    cmd_buffer_t cmds;
    struct
    {
        event_t buffer[DISPLAY_MAX_EVENT];
        uint64_t readIndex;
        uint64_t writeIndex;
    } events;
    list_t windows;
    surface_id_t newId;
} display_t;

uint64_t display_send_recieve_pattern(display_t* disp, cmd_header_t* cmd, event_t* event, event_type_t expected);

void display_cmds_push(display_t* disp, const cmd_header_t* cmd);

void display_cmds_flush(display_t* disp);

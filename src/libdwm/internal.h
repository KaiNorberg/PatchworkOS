#include <libdwm/dwm.h>

#include <sys/io.h>
#include <sys/list.h>

#include "grf.h"

typedef struct image
{
    list_entry_t entry;
    drawable_t draw;
} image_t;

typedef struct font
{
    list_entry_t entry;
    display_t* disp;
    grf_t grf;
} font_t;

typedef struct element
{
    list_entry_t entry;
    list_t children;
    struct element* parent;
    element_id_t id;
    procedure_t proc;
    window_t* win;
    void* private;
    rect_t rect;
} element_t;

element_t* element_new_root(window_t* win, element_id_t id, const rect_t* rect, procedure_t procedure, void* private);

typedef struct window
{
    list_entry_t entry;
    display_t* disp;
    surface_id_t surface;
    char name[MAX_NAME];
    rect_t rect;
    rect_t invalidRect;
    surface_type_t type;
    window_flags_t flags;
    char shmem[MAX_NAME];
    pixel_t* buffer;
    element_t* root;
    element_t* clientElement;
} window_t;

#define DISPLAY_MAX_EVENT 64

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
    list_t fonts;
    list_t images;
    surface_id_t newId;
    font_t* defaultFont;
} display_t;

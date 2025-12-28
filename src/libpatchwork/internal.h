#include <libpatchwork/patchwork.h>

#include <sys/io.h>
#include <sys/list.h>
#include <threads.h>

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
    element_flags_t flags;
    char* text;
    text_props_t textProps;
    image_t* image;
    image_props_t imageProps;
    theme_t theme;
} element_t;

element_t* element_new_root(window_t* win, element_id_t id, const rect_t* rect, const char* text, element_flags_t flags,
    procedure_t procedure, void* private);

typedef struct window
{
    list_entry_t entry;
    display_t* disp;
    char name[MAX_NAME];
    rect_t rect;
    rect_t invalidRect;
    surface_type_t type;
    window_flags_t flags;
    surface_id_t surface;
    pixel_t* buffer;
    element_t* root;
    element_t* clientElement;
} window_t;

#define DISPLAY_MAX_EVENT 64

typedef struct
{
    event_t buffer[DISPLAY_MAX_EVENT];
    uint64_t readIndex;
    uint64_t writeIndex;
} event_queue_t;

typedef struct display
{
    char* id;
    fd_t ctl;
    fd_t data;
    event_queue_t events;
    bool isConnected;
    cmd_buffer_t cmds;
    list_t windows;
    list_t fonts;
    list_t images;
    font_t* defaultFont;
    mtx_t mutex;
} display_t;

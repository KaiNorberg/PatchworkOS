#pragma once

#include "start_menu.h"

#include <libpatchwork/patchwork.h>
#include <sys/io.h>
#include <sys/list.h>

#define START_WIDTH 100
#define START_ID (UINT64_MAX - 10)

#define CLOCK_WIDTH 150

#define TASK_BUTTON_MAX_WIDTH 150

#define CLOCK_LABEL_ID (UINT64_MAX - 11)

typedef struct
{
    list_entry_t entry;
    surface_info_t info;
    char name[MAX_NAME];
    element_t* button;
} taskbar_entry_t;

typedef struct
{
    window_t* win;
    display_t* disp;
    window_t* startMenu;
    list_t entries;
} taskbar_t;

window_t* taskbar_new(display_t* disp);

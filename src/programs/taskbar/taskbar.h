#pragma once

#include "start_menu.h"

#include <libpatchwork/patchwork.h>

#define START_WIDTH 100
#define START_ID 0

#define CLOCK_WIDTH 150

#define UEVENT_CLOCK (UEVENT_BASE + 1)

#define CLOCK_LABEL_ID 1234

typedef struct
{
    window_t* win;
    display_t* disp;
    start_menu_t startMenu;
} taskbar_t;

void taskbar_init(taskbar_t* taskbar, display_t* disp);

void taskbar_deinit(taskbar_t* taskbar);
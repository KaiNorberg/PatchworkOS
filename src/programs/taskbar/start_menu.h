#pragma once

#include <libpatchwork/patchwork.h>
#include <stdbool.h>

#define START_BUTTON_HEIGHT 32

#define START_MENU_WIDTH 250

#define START_MENU_ANIMATION_TIME (CLOCKS_PER_SEC / 10)

#define EVENT_USER_START_MENU_CLOSE (EVENT_USER_START + 1)

typedef enum
{
    START_MENU_CLOSED,
    START_MENU_OPEN,
    START_MENU_CLOSING,
    START_MENU_OPENING,
} start_menu_state_t;

typedef struct
{
    window_t* win;
    window_t* taskbar;
    clock_t animationStartTime;
    start_menu_state_t state;
} start_menu_t;

window_t* start_menu_new(window_t* taskbar, display_t* disp);

void start_menu_open(window_t* startMenu);

void start_menu_close(window_t* startMenu);

start_menu_state_t start_menu_get_state(window_t* startMenu);

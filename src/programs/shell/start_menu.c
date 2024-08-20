#include "start_menu.h"

#include "_AUX/rect_t.h"
#include "shell.h"
#include "taskbar.h"

#include <sys/gfx.h>
#include <sys/win.h>

#define START_BUTTON_HEIGHT 32

#define START_MENU_WIDTH 250
#define START_MENU_HEIGHT 400

static win_t* startMenu = NULL;

typedef struct
{
    const char* name;
    const char* path;
} start_entry_t;

// TODO: Load this from config file.
static start_entry_t entries[] = {
    {.name = "Calculator", .path = "home:/usr/bin/calc"},
    {.name = "Terminal", .path = "home:/usr/bin/terminal"},
};

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        rect_t clientRect;
        win_client_rect(window, &clientRect);

        win_text_prop_t props = {.height = 16, .xAlign = GFX_CENTER, .yAlign = GFX_CENTER, .foreground = 0xFF000000};

        for (uint64_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
        {
            rect_t rect =
                RECT_INIT_DIM(winTheme.edgeWidth * 2, winTheme.edgeWidth + winTheme.edgeWidth * (i + 1) + i * START_BUTTON_HEIGHT,
                    RECT_WIDTH(&clientRect) - winTheme.edgeWidth * 4, START_BUTTON_HEIGHT);
            win_button_new(window, entries[i].name, &rect, i, &props, WIN_BUTTON_NONE);
        }
    }
    break;
    case LMSG_REDRAW:
    {
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        rect_t rect = RECT_INIT_GFX(&gfx);

        gfx_edge(&gfx, &rect, winTheme.edgeWidth, winTheme.bright, winTheme.dark);
        RECT_SHRINK(&rect, winTheme.edgeWidth);
        gfx_rect(&gfx, &rect, winTheme.background);

        win_draw_end(window, &gfx);
    }
    break;
    case LMSG_COMMAND:
    {
        lmsg_command_t* data = (lmsg_command_t*)msg->data;
        if (data->type == LMSG_COMMAND_RELEASE)
        {
            const char* argv[] = {entries[data->id].path, NULL};
            if (spawn(argv, NULL) == ERR)
            {
                //TODO: Add err handling, msg box?
            }
        }
    }
    break;
    }

    return 0;
}

void start_menu_open(void)
{
    if (startMenu != NULL)
    {
        return;
    }

    rect_t screenRect;
    win_screen_rect(&screenRect);

    rect_t rect =
        RECT_INIT_DIM(0, RECT_HEIGHT(&screenRect) - TOPBAR_HEIGHT - START_MENU_HEIGHT, START_MENU_WIDTH, START_MENU_HEIGHT);
    startMenu = win_new("StartMenu", &rect, DWM_WINDOW, WIN_NONE, procedure);

    shell_push(startMenu);
}

void start_menu_close(void)
{
    if (startMenu == NULL)
    {
        return;
    }

    win_send(startMenu, LMSG_QUIT, NULL, 0);
    startMenu = NULL;
}

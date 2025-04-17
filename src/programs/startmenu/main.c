#include <stdio.h>
#include <sys/gfx.h>
#include <sys/win.h>

#define TOPBAR_HEIGHT 43

#define START_BUTTON_HEIGHT 32

#define START_MENU_WIDTH 250
#define START_MENU_HEIGHT 400

#define START_MENU_SIDE_BAR_WIDTH 32

#define START_MENU_SHUT_DOWN_ID 100
#define START_MENU_RESTART_ID 101

static win_t* startMenu = NULL;

typedef struct
{
    const char* name;
    const char* path;
} start_entry_t;

// TODO: Load this from config file.
static start_entry_t entries[] = {
    {.name = "Calculator", .path = "home:/usr/bin/calculator"},
    {.name = "Terminal", .path = "home:/usr/bin/terminal"},
    {.name = "Tetris", .path = "home:/usr/bin/tetris"},
    {.name = "Error Test", .path = "this:/is/a/nonsense/file/path"},
};

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        rect_t clientRect;
        win_client_rect(window, &clientRect);

        win_text_prop_t props = {.height = 16,
            .xAlign = GFX_CENTER,
            .yAlign = GFX_CENTER,
            .foreground = winTheme.dark,
            winTheme.background};

        for (uint64_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
        {
            rect_t rect = RECT_INIT(winTheme.edgeWidth * 2 + START_MENU_SIDE_BAR_WIDTH,
                winTheme.edgeWidth + winTheme.edgeWidth * (i + 1) + i * START_BUTTON_HEIGHT,
                RECT_WIDTH(&clientRect) - winTheme.edgeWidth * 2,
                winTheme.edgeWidth + winTheme.edgeWidth * (i + 1) + i * START_BUTTON_HEIGHT + START_BUTTON_HEIGHT);
            win_button_new(window, entries[i].name, &rect, i, &props, WIN_BUTTON_NONE);
        }

        /*rect_t shutdownRect = RECT_INIT();
        win_button_new(window, entries[i].name, &shutdownRect, START_MENU_SHUT_DOWN_ID, &props, WIN_BUTTON_NONE);

        rect_t restartRect = RECT_INIT()
        win_button_new(window, entries[i].name, &rect, START_MENU_RESTART_ID, &props, WIN_BUTTON_NONE);*/
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

        rect_t sideBar = RECT_INIT(winTheme.edgeWidth + winTheme.padding, winTheme.edgeWidth + winTheme.padding,
            winTheme.edgeWidth + winTheme.padding + START_MENU_SIDE_BAR_WIDTH,
            START_MENU_HEIGHT - winTheme.edgeWidth - winTheme.padding);
        gfx_rect(&gfx, &sideBar, winTheme.unSelected);

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
                char buffer[MAX_PATH];
                sprintf(buffer, "Failed to spawn process (%s)!", entries[data->id].path);

                win_popup(buffer, "Error!", POPUP_TYPE_OK, NULL);
            }
        }
    }
    break;
    }

    return 0;
}

int main(void)
{
    rect_t screenRect;
    win_screen_rect(&screenRect);

    rect_t rect =
        RECT_INIT_DIM(0, RECT_HEIGHT(&screenRect) - TOPBAR_HEIGHT - START_MENU_HEIGHT, START_MENU_WIDTH, START_MENU_HEIGHT);
    win_t* window = win_new("StartMenu", &rect, DWM_WINDOW, WIN_NONE, procedure);

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    return 0;
}

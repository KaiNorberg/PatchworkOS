#include "terminal.h"
#include "sys/dwm.h"
#include "sys/gfx.h"
#include "sys/kbd.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/win.h>

#define UMSG_BLINK (UMSG_BASE + 1)

#define BLINK_INTERVAL (SEC / 2)

#define TERMINAL_WIDTH 750
#define TERMINAL_HEIGHT 500

static win_t* terminal;

static point_t cursorPos;
static bool cursorVisible;

#define CURSOR_POS_TO_CLIENT_POS(cursorPos, font) \
    (point_t){ \
        .x = ((cursorPos)->x * (font)->width) + winTheme.edgeWidth + winTheme.padding, \
        .y = ((cursorPos)->y * (font)->height) + winTheme.edgeWidth + winTheme.padding, \
    };

static void terminal_draw_cursor(void)
{
    const gfx_psf_t* font = win_font(terminal);
    point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, font);

    gfx_t gfx;
    win_draw_begin(terminal, &gfx);
    if (cursorVisible)
    {
        gfx_char(&gfx, font, &point, font->height, ' ', winTheme.bright, winTheme.bright);
    }
    else
    {
        gfx_char(&gfx, font, &point, font->height, ' ', winTheme.dark, winTheme.dark);
    }
    win_draw_end(terminal, &gfx);
}

uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        win_timer_set(window, BLINK_INTERVAL);
    }
    break;
    case LMSG_REDRAW:
    {
        cursorVisible = false;

        gfx_t gfx;
        win_draw_begin(window, &gfx);

        rect_t rect = RECT_INIT_GFX(&gfx);
        gfx_edge(&gfx, &rect, winTheme.edgeWidth, winTheme.shadow, winTheme.highlight);
        RECT_SHRINK(&rect, winTheme.edgeWidth);
        gfx_rect(&gfx, &rect, winTheme.dark);

        win_draw_end(window, &gfx);

        cursorPos = (point_t){.x = 0, .y = 0};
        terminal_print("Welcome to the Terminal (Very WIP)\n");
        //terminal_print("Type help for a list of commands\n\n");

        terminal_print("home:/usr/bin\n> ");
    }
    break;
    case LMSG_TIMER:
    {
        win_timer_set(window, BLINK_INTERVAL);

        cursorVisible = !cursorVisible;
        terminal_draw_cursor();
    }
    break;
    /*case MSG_KBD:
    {
        msg_kbd_t* data = (msg_kbd_t*)msg->data;

        if (data->type == KBD_PRESS)
        {
            terminal_put(data->code);
        }

        cursorVisible = true;
        terminal_draw_cursor();
        win_timer_set(window, BLINK_INTERVAL);
    }
    break;*/
    }

    return 0;
}

void terminal_init(void)
{
    rect_t rect = RECT_INIT_DIM(500, 200, TERMINAL_WIDTH, TERMINAL_HEIGHT);
    win_expand_to_window(&rect, WIN_DECO);

    terminal = win_new("Terminal", &rect, DWM_WINDOW, WIN_DECO, procedure);
    if (terminal == NULL)
    {
        exit(errno);
    }
}

void terminal_cleanup(void)
{
    win_free(terminal);
}

void terminal_loop(void)
{
    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(terminal, &msg, NEVER);
        win_dispatch(terminal, &msg);
    }
}

static void terminal_put_backend(gfx_t* gfx, char chr)
{
    const gfx_psf_t* font = win_font(terminal);

    switch (chr)
    {
    case '\n':
    {
        cursorPos.x = 0;
        cursorPos.y++;
    }
    break;
    case '\b':
    {
        if (cursorPos.x != 0)
        {
            cursorPos.x--;
        }
    }
    break;
    default:
    {
        point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, font);
        gfx_char(gfx, font, &point, font->height, chr, winTheme.bright, winTheme.dark);
        cursorPos.x++;

        if ((cursorPos.x + 1) * font->width > gfx->width - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_put_backend(gfx, '\n');
        }
    }
    break;
    }
}

void terminal_put(char chr)
{
    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    terminal_put_backend(&gfx, chr);

    win_draw_end(terminal, &gfx);
}

void terminal_print(const char* str)
{
    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    while (*str != '\0')
    {
        terminal_put_backend(&gfx, *str);
        str++;
    }

    win_draw_end(terminal, &gfx);
}

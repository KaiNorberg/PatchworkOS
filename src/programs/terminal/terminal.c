#include "terminal.h"
#include "command.h"
#include "sys/gfx.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/win.h>

#define UMSG_BLINK (UMSG_BASE + 1)

#define BLINK_INTERVAL (SEC / 2)

#define TERMINAL_WIDTH 750
#define TERMINAL_HEIGHT 500

static win_t* terminal;
static terminal_state_t state;

static point_t cursorPos;
static bool cursorVisible;

static char command[MAX_PATH];

#define CURSOR_POS_TO_CLIENT_POS(cursorPos, font) \
    (point_t){ \
        .x = ((cursorPos)->x * (font)->width) + winTheme.edgeWidth + winTheme.padding, \
        .y = ((cursorPos)->y * (font)->height) + winTheme.edgeWidth + winTheme.padding, \
    };

static void terminal_cursor_draw(void)
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

static void terminal_cursor_clear(void)
{
    bool temp = cursorVisible;
    cursorVisible = false;
    terminal_cursor_draw();
    cursorVisible = temp;
}

static void terminal_print_prompt(void)
{
    char cwd[MAX_PATH];
    realpath(cwd, ".");

    terminal_print("\n");
    terminal_print(cwd);
    terminal_print("\n> ");
}

static void terminal_scroll(void)
{
    cursorPos.y -= 2;

    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    rect_t rect = RECT_INIT_GFX(&gfx);
    RECT_SHRINK(&rect, winTheme.edgeWidth);

    gfx_scroll(&gfx, &rect, win_font(terminal)->height * 2, winTheme.dark);

    win_draw_end(terminal, &gfx);
}

static void terminal_handle_input_command(char chr)
{
    switch (chr)
    {
    case '\n':
    {
        terminal_put('\n');
        command_parse(command);
        command[0] = '\0';

        terminal_print_prompt();
    }
    break;
    case '\b':
    {
        uint64_t strLen = strlen(command);
        if (strLen != 0)
        {
            command[strLen - 1] = '\0';
            terminal_put('\b');
        }
    }
    break;
    default:
    {
        uint64_t strLen = strlen(command);
        if (strLen >= MAX_PATH - 2)
        {
            break;
        }
        command[strLen] = chr;
        command[strLen + 1] = '\0';

        terminal_put(chr);
    }
    break;
    }
}

uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        command[0] = '\0';
        win_timer_set(window, BLINK_INTERVAL);

        chdir("home:/usr");
    }
    break;
    case LMSG_REDRAW:
    {
        terminal_clear();

        terminal_print("Welcome to the Terminal (Very WIP)\n");
        terminal_print("Type help for a list of commands\n");

        terminal_print_prompt();
    }
    break;
    case LMSG_TIMER:
    {
        win_timer_set(window, BLINK_INTERVAL);

        cursorVisible = !cursorVisible;
        terminal_cursor_draw();
    }
    break;
    case MSG_KBD:
    {
        msg_kbd_t* data = (msg_kbd_t*)msg->data;
        if (data->type != KBD_PRESS)
        {
            break;
        }

        char chr = kbd_ascii(data->code, data->mods);
        if (chr == '\0')
        {
            break;
        }

        switch (state)
        {
        case TERMINAL_COMMAND:
        {
            terminal_handle_input_command(kbd_ascii(data->code, data->mods));
        }
        break;
        }

        cursorVisible = true;
        terminal_cursor_draw();
        win_timer_set(window, BLINK_INTERVAL);
    }
    break;
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

void terminal_clear(void)
{
    cursorVisible = false;
    cursorPos = (point_t){.x = 0, .y = 0};

    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    rect_t rect = RECT_INIT_GFX(&gfx);
    gfx_edge(&gfx, &rect, winTheme.edgeWidth, winTheme.shadow, winTheme.highlight);
    RECT_SHRINK(&rect, winTheme.edgeWidth);
    gfx_rect(&gfx, &rect, winTheme.dark);

    win_draw_end(terminal, &gfx);
}

static void terminal_put_backend(gfx_t* gfx, char chr)
{
    const gfx_psf_t* font = win_font(terminal);

    switch (chr)
    {
    case '\n':
    {
        terminal_cursor_clear();
        cursorPos.x = 0;
        cursorPos.y++;

        if ((cursorPos.y + 1) * font->height > gfx->height - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_scroll();
        }
    }
    break;
    case '\b':
    {
        if (cursorPos.x != 0)
        {
            terminal_cursor_clear();
            cursorPos.x--;
            terminal_cursor_clear();
        }
    }
    break;
    default:
    {
        point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, font);
        gfx_char(gfx, font, &point, font->height, chr, winTheme.bright, winTheme.dark);
        cursorPos.x++;

        if ((cursorPos.x + 2) * font->width > gfx->width - winTheme.edgeWidth * 2 - winTheme.padding * 2)
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

void terminal_error(const char* str)
{
    terminal_print("error: ");
    if (str != NULL)
    {
        terminal_print(str);
    }
    else
    {
        terminal_print(strerror(errno));
    }
    terminal_put('\n');
}

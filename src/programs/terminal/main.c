#include "_AUX/rect_t.h"
#include <ctype.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <sys/win.h>
#include <threads.h>

#define UMSG_BLINK (UMSG_BASE + 1)

#define BLINK_INTERVAL (SEC / 2)

#define TERMINAL_WIDTH 750
#define TERMINAL_HEIGHT 500

static win_t* terminal;

static point_t cursorPos;
static bool cursorVisible;

static fd_t stdin[2];
static fd_t stdout[2];

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

static void terminal_clear(void)
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

static void terminal_scroll(void)
{
    gfx_t gfx;
    win_draw_begin(terminal, &gfx);
    cursorPos.y -= 2;

    rect_t rect = RECT_INIT_GFX(&gfx);
    RECT_SHRINK(&rect, winTheme.edgeWidth);

    gfx_scroll(&gfx, &rect, win_font(terminal)->height * 2, winTheme.dark);
    win_draw_end(terminal, &gfx);
}

static void terminal_put(char chr)
{
    const gfx_psf_t* font = win_font(terminal);

    terminal_cursor_clear();

    rect_t rect;
    win_client_rect(terminal, &rect);

    switch (chr)
    {
    case '\n':
    {
        cursorPos.x = 0;
        cursorPos.y++;

        if ((cursorPos.y + 1) * font->height > RECT_HEIGHT(&rect) - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_scroll();
        }
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
    case '\0':
    {
    }
    break;
    default:
    {
        gfx_t gfx;
        win_draw_begin(terminal, &gfx);
        point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, font);
        gfx_char(&gfx, font, &point, font->height, chr, winTheme.bright, winTheme.dark);
        cursorPos.x++;
        win_draw_end(terminal, &gfx);

        if ((cursorPos.x + 2) * font->width > RECT_WIDTH(&rect) - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_put('\n');
        }
    }
    break;
    }

    cursorVisible = true;
    terminal_cursor_draw();
    win_timer_set(terminal, BLINK_INTERVAL);
}

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    static char buffer[MAX_PATH];
    static uint64_t index;

    switch (msg->type)
    {
    case LMSG_INIT:
    {
        index = 0;
        buffer[0] = '\0';
        win_timer_set(window, BLINK_INTERVAL);
    }
    break;
    case LMSG_REDRAW:
    {
        terminal_clear();
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
        switch (chr)
        {
        case '\n':
        {
            buffer[index] = '\0';
            writef(stdin[PIPE_WRITE], "%s\n", buffer);
            index = 0;
            terminal_put(chr);
        }
        break;
        case '\b':
        {
            if (index != 0)
            {
                --index;
                terminal_put(chr);
            }
        }
        break;
        default:
        {
            if (index < MAX_PATH)
            {
                buffer[index++] = chr;
                terminal_put(chr);
            }
        }
        break;
        }
    }
    break;
    }

    return 0;
}

int main(void)
{
    rect_t rect = RECT_INIT_DIM(500, 200, TERMINAL_WIDTH, TERMINAL_HEIGHT);
    win_expand_to_window(&rect, WIN_DECO);

    terminal = win_new("Terminal", &rect, DWM_WINDOW, WIN_DECO, procedure);
    if (terminal == NULL)
    {
        exit(errno);
    }

    open2("sys:/pipe/new", stdin);
    open2("sys:/pipe/new", stdout);
    dup2(stdin[PIPE_READ], STDIN_FILENO);
    dup2(stdout[PIPE_WRITE], STDOUT_FILENO);

    const char* argv[] = {"home:/usr/bin/shell", NULL};
    spawn_fd_t fds[] = {
        {.child = STDIN_FILENO, .parent = STDIN_FILENO},
        {.child = STDOUT_FILENO, .parent = STDOUT_FILENO},
        SPAWN_FD_END,
    };
    fd_t shell = pid_open(spawn(argv, fds), "ctl");

    bool shouldQuit = false;
    while (!shouldQuit)
    {
        pollfd_t fds[] = {{.fd = stdout[PIPE_READ], .requested = POLL_READ}, {.fd = win_fd(terminal), .requested = POLL_READ}};
        poll(fds, 2, BLINK_INTERVAL);

        msg_t msg = {0};
        while (win_receive(terminal, &msg, 0))
        {
            if (msg.type == LMSG_QUIT)
            {
                shouldQuit = true;
            }
            win_dispatch(terminal, &msg);
        }

        if (fds[0].occurred == POLL_READ)
        {
            while (poll1(stdout[PIPE_READ], POLL_READ, 0) == POLL_READ)
            {
                char chr;
                read(stdout[PIPE_READ], &chr, 1);
                terminal_put(chr);
            }
        }
    }

    writef(shell, "kill");
    close(shell);

    win_free(terminal);
    return 0;
}

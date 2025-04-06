#include "terminal.h"
#include "command.h"

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

// This is probobly one of the messiest parts of this project.

#define UMSG_BLINK (UMSG_BASE + 1)

#define BLINK_INTERVAL (SEC / 2)

#define TERMINAL_WIDTH 750
#define TERMINAL_HEIGHT 500

static win_t* terminal;

static point_t cursorPos;
static bool cursorVisible;

static thrd_t thread;

static fd_t printPipe;
static fd_t kbdPipe;

static atomic_bool shouldQuit;
static atomic_bool hasQuit;

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

        write(kbdPipe, &chr, 1);
    }
    break;
    }

    return 0;
}

static void terminal_scroll(gfx_t* gfx)
{
    cursorPos.y -= 2;

    rect_t rect = RECT_INIT_GFX(gfx);
    RECT_SHRINK(&rect, winTheme.edgeWidth);

    gfx_scroll(gfx, &rect, win_font(terminal)->height * 2, winTheme.dark);
}

static void terminal_put(gfx_t* gfx, char chr)
{
    const gfx_psf_t* font = win_font(terminal);

    terminal_cursor_clear();

    switch (chr)
    {
    case '\n':
    {
        cursorPos.x = 0;
        cursorPos.y++;

        if ((cursorPos.y + 1) * font->height > gfx->height - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_scroll(gfx);
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
        point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, font);
        gfx_char(gfx, font, &point, font->height, chr, winTheme.bright, winTheme.dark);
        cursorPos.x++;

        if ((cursorPos.x + 2) * font->width > gfx->width - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_put(gfx, '\n');
        }
    }
    break;
    }

    cursorVisible = true;
    terminal_cursor_draw();
    win_timer_set(terminal, BLINK_INTERVAL);
}

int terminal_loop(void* data)
{
    msg_t msg = {0};
    while (msg.type != LMSG_QUIT && !shouldQuit)
    {
        pollfd_t fds[] = {{.fd = printPipe, .requested = POLL_READ}, {.fd = win_fd(terminal), .requested = POLL_READ}};
        poll(fds, 2, BLINK_INTERVAL);

        while (win_receive(terminal, &msg, 0))
        {
            win_dispatch(terminal, &msg);
        }

        if (fds[0].occurred == POLL_READ)
        {
            gfx_t gfx;
            win_draw_begin(terminal, &gfx);

            while (poll1(printPipe, POLL_READ, 0) == POLL_READ)
            {
                char chr;
                read(printPipe, &chr, 1);
                terminal_put(&gfx, chr);
            }

            win_draw_end(terminal, &gfx);
        }
    }

    atomic_store(&hasQuit, true);
    exit(EXIT_SUCCESS);
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

    printPipe = open("sys:/pipe/new");
    kbdPipe = open("sys:/pipe/new");
    
    atomic_init(&shouldQuit, false);
    atomic_init(&hasQuit, false);

    thrd_create(&thread, terminal_loop, NULL);
}

void terminal_deinit(void)
{
    atomic_store(&shouldQuit, true);
    while (!atomic_load(&hasQuit))
    {
        asm volatile("pause");
    }
    close(printPipe);
    close(kbdPipe);
    win_free(terminal);
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

char terminal_input(void)
{
    char chr;
    read(kbdPipe, &chr, 1);
    return chr;
}

void terminal_print(const char* str, ...)
{
    char buffer[MAX_PATH];
    va_list args;
    va_start(args, str);
    vsprintf(buffer, str, args);
    va_end(args);

    write(printPipe, buffer, strlen(buffer));

    /*if (str == NULL)
    {
        return;
    }

    gfx_t gfx;
    win_draw_begin(terminal, &gfx);

    char buffer[MAX_PATH];
    va_list args;
    va_start(args, str);
    vsprintf(buffer, str, args);
    va_end(args);

    char* chr = buffer;
    while (*chr != '\0')
    {
        terminal_put(&gfx, *chr++);
    }

    win_draw_end(terminal, &gfx);*/
}

void terminal_error(const char* str, ...)
{
    if (str != NULL)
    {
        char buffer[MAX_PATH];
        va_list args;
        va_start(args, str);
        vsprintf(buffer, str, args);
        va_end(args);

        terminal_print("error: %s\n", buffer);
    }
    else
    {
        terminal_print("error: %s\n", strerror(errno));
    }
}

#include "terminal.h"
#include "sys/io.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/win.h>

static void terminal_cursor_draw(terminal_t* term)
{
    const gfx_psf_t* font = win_font(term->win);
    point_t point = CURSOR_POS_TO_CLIENT_POS(&term->cursorPos, font);

    gfx_t gfx;
    win_draw_begin(term->win, &gfx);

    if (term->cursorVisible)
    {
        gfx_char(&gfx, font, &point, font->height, ' ', winTheme.bright, winTheme.bright);
    }
    else
    {
        gfx_char(&gfx, font, &point, font->height, ' ', winTheme.dark, winTheme.dark);
    }

    win_draw_end(term->win, &gfx);
}

static void terminal_cursor_clear(terminal_t* term)
{
    bool temp = term->cursorVisible;
    term->cursorVisible = false;
    terminal_cursor_draw(term);
    term->cursorVisible = temp;
}

static void terminal_clear(terminal_t* term)
{
    term->cursorVisible = false;
    term->cursorPos = (point_t){0, 0};

    gfx_t gfx;
    win_draw_begin(term->win, &gfx);

    rect_t rect = RECT_INIT_GFX(&gfx);
    gfx_edge(&gfx, &rect, winTheme.edgeWidth, winTheme.shadow, winTheme.highlight);
    RECT_SHRINK(&rect, winTheme.edgeWidth);
    gfx_rect(&gfx, &rect, winTheme.dark);

    win_draw_end(term->win, &gfx);

    term->cursorVisible = true;
    terminal_cursor_draw(term);
}

static void terminal_scroll(terminal_t* term)
{
    gfx_t gfx;
    win_draw_begin(term->win, &gfx);
    term->cursorPos.y -= 2;

    rect_t rect = RECT_INIT_GFX(&gfx);
    RECT_SHRINK(&rect, winTheme.edgeWidth);

    gfx_scroll(&gfx, &rect, win_font(term->win)->height * 2, winTheme.dark);
    win_draw_end(term->win, &gfx);
}

static void terminal_put(terminal_t* term, char chr)
{
    const gfx_psf_t* font = win_font(term->win);

    terminal_cursor_clear(term);

    rect_t rect;
    win_client_rect(term->win, &rect);

    switch (chr)
    {
    case '\n':
    {
        term->cursorPos.x = 0;
        term->cursorPos.y++;

        if ((term->cursorPos.y + 1) * font->height > RECT_HEIGHT(&rect) - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_scroll(term);
        }
    }
    break;
    case '\b':
    {
        if (term->cursorPos.x != 0)
        {
            term->cursorPos.x--;
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
        win_draw_begin(term->win, &gfx);
        point_t point = CURSOR_POS_TO_CLIENT_POS(&term->cursorPos, font);
        gfx_char(&gfx, font, &point, font->height, chr, winTheme.bright, winTheme.dark);
        term->cursorPos.x++;
        win_draw_end(term->win, &gfx);

        if ((term->cursorPos.x + 2) * font->width > RECT_WIDTH(&rect) - winTheme.edgeWidth * 2 - winTheme.padding * 2)
        {
            terminal_put(term, '\n');
        }
    }
    break;
    }

    term->cursorVisible = true;
    terminal_cursor_draw(term);
    win_timer_set(term->win, BLINK_INTERVAL);
}

static void terminal_handle_input(terminal_t* term, char chr)
{
    switch (chr)
    {
    case '\n':
    {
        term->inputBuffer[term->inputIndex] = '\0';
        writef(term->stdin[PIPE_WRITE], "%s\n", term->inputBuffer);
        term->inputIndex = 0;
        terminal_put(term, chr);
    }
    break;
    case '\b':
    {
        if (term->inputIndex > 0)
        {
            term->inputIndex--;
            terminal_put(term, chr);
        }
    }
    break;
    default:
    {
        if (term->inputIndex < MAX_PATH)
        {
            term->inputBuffer[term->inputIndex++] = chr;
            terminal_put(term, chr);
        }
    }
    break;
    }
}

static uint64_t terminal_procedure(win_t* win, const msg_t* msg)
{
    terminal_t* term = win_private(win);

    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        terminal_clear(term);
    }
    break;
    case LMSG_TIMER:
    {
        win_timer_set(win, BLINK_INTERVAL);

        term->cursorVisible = !term->cursorVisible;
        terminal_cursor_draw(term);
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
        if (chr != '\0')
        {
            terminal_handle_input(term, chr);
        }
    }
    break;
    }

    return 0;
}

void terminal_init(terminal_t* term)
{
    rect_t rect = RECT_INIT_DIM(500, 200, TERMINAL_WIDTH, TERMINAL_HEIGHT);
    win_expand_to_window(&rect, WIN_DECO);

    term->win = win_new("Terminal", &rect, DWM_WINDOW, WIN_DECO, terminal_procedure);
    if (term == NULL)
    {
        exit(errno);
    }
    win_private_set(term->win, term);
    win_timer_set(term->win, BLINK_INTERVAL);

    term->cursorPos = (point_t){0};
    term->cursorVisible = false;
    term->inputBuffer[0] = '\0';
    term->inputIndex = 0;

    if (open2("sys:/pipe/new", term->stdin) == ERR || open2("sys:/pipe/new", term->stdout) == ERR)
    {
        win_free(term->win);
        exit(errno);
    }

    const char* argv[] = {"home:/usr/bin/shell", NULL};
    spawn_fd_t fds[] = {
        {.child = STDIN_FILENO, .parent = term->stdin[PIPE_READ]},
        {.child = STDOUT_FILENO, .parent = term->stdout[PIPE_WRITE]},
        SPAWN_FD_END,
    };
    pid_t shell = spawn(argv, fds);
    if (shell == ERR)
    {
        close(term->stdin[0]);
        close(term->stdin[1]);
        close(term->stdout[0]);
        close(term->stdout[1]);
        win_free(term->win);
        exit(errno);
    }

    term->shellCtl = openf("sys:/proc/%d/ctl", shell);
    if (term->shellCtl == ERR)
    {
        close(term->stdin[0]);
        close(term->stdin[1]);
        close(term->stdout[0]);
        close(term->stdout[1]);
        win_free(term->win);
        exit(errno);
    }
}

void terminal_deinit(terminal_t* term)
{
    writef(term->shellCtl, "kill");
    close(term->shellCtl);

    close(term->stdin[0]);
    close(term->stdin[1]);
    close(term->stdout[0]);
    close(term->stdout[1]);

    win_free(term->win);
}

bool terminal_update(terminal_t* term)
{
    pollfd_t fds[] = {{.fd = term->stdout[PIPE_READ], .requested = POLL_READ}, {.fd = win_fd(term->win), .requested = POLL_READ}};
    poll(fds, 2, BLINK_INTERVAL);

    bool shouldQuit = false;
    msg_t msg = {0};
    while (win_receive(term->win, &msg, 0))
    {
        if (msg.type == LMSG_QUIT)
        {
            shouldQuit = true;
        }
        win_dispatch(term->win, &msg);
    }

    if (fds[0].occurred == POLL_READ)
    {
        while (poll1(term->stdout[PIPE_READ], POLL_READ, 0) == POLL_READ)
        {
            char chr;
            read(term->stdout[PIPE_READ], &chr, 1);
            terminal_put(term, chr);
        }
    }

    return shouldQuit;
}

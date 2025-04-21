#include "terminal.h"
#include "history.h"
#include "input.h"
#include "sys/io.h"
#include "sys/kbd.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/win.h>

static void terminal_cursor_draw(terminal_t* term)
{
    const gfx_psf_t* font = win_font(term->win);

    char cursor  = ' ';
    point_t cursorPos = term->cursorPos;
    if (term->input.length != 0)
    {
        if (term->input.index != term->input.length)
        {
            cursor = term->input.buffer[term->input.index];
        }
        cursorPos.x += term->input.index;
    }
    point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, font);

    gfx_t gfx;
    win_draw_begin(term->win, &gfx);
    if (term->cursorVisible)
    {
        gfx_char(&gfx, font, &point, font->height, cursor, winTheme.dark, winTheme.bright);
    }
    else
    {
        gfx_char(&gfx, font, &point, font->height, cursor, winTheme.bright, winTheme.dark);
    }
    win_draw_end(term->win, &gfx);
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

static void terminal_scroll(terminal_t* term, gfx_t* gfx)
{
    term->cursorPos.y -= 1;

    rect_t rect = RECT_INIT_GFX(gfx);
    RECT_SHRINK(&rect, winTheme.edgeWidth);

    gfx_scroll(gfx, &rect, win_font(term->win)->height, winTheme.dark);
}

static void terminal_draw_char(terminal_t* term, gfx_t* gfx, const point_t* point, char chr)
{
    const gfx_psf_t* font = win_font(term->win);
    point_t clientPos = CURSOR_POS_TO_CLIENT_POS(point, font);
    gfx_char(gfx, font, &clientPos, font->height, chr, winTheme.bright, winTheme.dark);
}

static void terminal_put(terminal_t* term, gfx_t* gfx, char chr)
{
    const gfx_psf_t* font = win_font(term->win);
    rect_t rect;
    win_client_rect(term->win, &rect);

    switch (chr)
    {
    case '\n':
    {
        if (term->cursorVisible)
        {
            term->cursorVisible = false;
            terminal_cursor_draw(term);
        }

        term->cursorPos.x = 0;
        term->cursorPos.y++;

        if (CURSOR_POS_Y_OUT_OF_BOUNDS(term->cursorPos.y + 1, font))
        {
            terminal_scroll(term, gfx);
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
    default:
    {
        terminal_draw_char(term, gfx, &term->cursorPos, chr);
        term->cursorPos.x++;
        term->cursorVisible = false;

        if (CURSOR_POS_X_OUT_OF_BOUNDS(term->cursorPos.x + 1, font))
        {
            terminal_put(term, gfx, '\n');
        }
    }
    break;
    }
}

static void terminal_write(terminal_t* term, const char* str)
{
    gfx_t gfx;
    win_draw_begin(term->win, &gfx);
    const char* chr = str;
    while (*chr != '\0')
    {
        terminal_put(term, &gfx, *chr);
        chr++;
    }
    win_draw_end(term->win, &gfx);

    term->cursorVisible = true;
    terminal_cursor_draw(term);
    win_timer_set(term->win, BLINK_INTERVAL);
}

static void terminal_redraw_input(terminal_t* term, uint64_t prevLength)
{
    gfx_t gfx;
    win_draw_begin(term->win, &gfx);
    point_t point = term->cursorPos;
    for (uint64_t i = 0; i < term->input.length; i++)
    {
        terminal_draw_char(term, &gfx, &point, term->input.buffer[i]);
        point.x++;
    }
    for (uint64_t i = term->input.length; i < prevLength + 1; i++)
    {
        terminal_draw_char(term, &gfx, &point, ' ');
        point.x++;
    }
    win_draw_end(term->win, &gfx);

    term->cursorVisible = true;
    terminal_cursor_draw(term);
    win_timer_set(term->win, BLINK_INTERVAL);
}

static void terminal_handle_input(terminal_t* term, keycode_t key, kbd_mods_t mods)
{
    uint64_t prevLength = term->input.length;

    switch (key)
    {
    case KEY_UP:
    {
        if (term->history.index == term->history.count)
        {
            input_save(&term->input);
        }

        const char* prev = history_previous(&term->history);
        if (prev != NULL)
        {
            input_set(&term->input, prev);
            terminal_redraw_input(term, prevLength);
        }
    }
    break;
    case KEY_DOWN:
    {
        const char* next = history_next(&term->history);
        if (next != NULL)
        {
            input_set(&term->input, next);
        }
        else
        {
            input_restore(&term->input);
        }
        terminal_redraw_input(term, prevLength);
    }
    break;
    case KEY_LEFT:
    {
        if (input_move(&term->input, -1) != ERR)
        {
            terminal_redraw_input(term, prevLength);
        }
    }
    break;
    case KEY_RIGHT:
    {
        if (input_move(&term->input, +1) != ERR)
        {
            terminal_redraw_input(term, prevLength);
        }
    }
    break;
    case KEY_ENTER:
    {
        writef(term->stdin[PIPE_WRITE], "%s\n", term->input.buffer);

        term->cursorVisible = false;
        terminal_cursor_draw(term);

        history_push(&term->history, term->input.buffer);
        input_set(&term->input, "");
        terminal_write(term, "\n");
    }
    break;
    case KEY_BACKSPACE:
    {
        input_backspace(&term->input);
        terminal_redraw_input(term, prevLength);
    }
    break;
    default:
    {
        char ascii = kbd_ascii(key, mods);
        if (ascii == '\0')
        {
            break;
        }

        if (CURSOR_POS_X_OUT_OF_BOUNDS(term->cursorPos.x + (int64_t)term->input.length + 2, win_font(term->win)))
        {
            break;
        }

        input_insert(&term->input, ascii);
        terminal_redraw_input(term, prevLength);
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
        if (data->type != KBD_PRESS || data->code == 0)
        {
            break;
        }
        terminal_handle_input(term, data->code, data->mods);
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
    input_init(&term->input);
    history_init(&term->history);

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
    input_deinit(&term->input);
    history_deinit(&term->history);

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
            char buffer[2] = {0, '\0'};
            read(term->stdout[PIPE_READ], buffer, 1);
            terminal_write(term, buffer);
            input_set(&term->input, "");
        }
    }

    return !shouldQuit;
}

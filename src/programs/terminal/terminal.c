#include "terminal.h"
#include "history.h"
#include "input.h"
#include "sys/io.h"
#include "sys/kbd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static void terminal_cursor_draw(terminal_t* term, element_t* elem)
{
    char cursor = ' ';
    point_t cursorPos = term->cursorPos;
    if (term->input.length != 0)
    {
        if (term->input.index != term->input.length)
        {
            cursor = term->input.buffer[term->input.index];
        }
        cursorPos.x += term->input.index;
    }
    point_t point = CURSOR_POS_TO_CLIENT_POS(&cursorPos, term->font);

    if (term->cursorVisible)
    {
        element_draw_string(elem, term->font, &point, windowTheme.dark, windowTheme.bright, &cursor, 1);
    }
    else
    {
        element_draw_string(elem, term->font, &point, windowTheme.bright, windowTheme.dark, &cursor, 1);
    }
}

static void terminal_clear(terminal_t* term, element_t* elem)
{
    term->cursorVisible = false;
    term->cursorPos = (point_t){0, 0};

    rect_t rect;
    element_content_rect(elem, &rect);

    element_draw_edge(elem, &rect, windowTheme.edgeWidth, windowTheme.shadow, windowTheme.highlight);
    RECT_SHRINK(&rect, windowTheme.edgeWidth);
    element_draw_rect(elem, &rect, windowTheme.dark);

    term->cursorVisible = true;
    terminal_cursor_draw(term, elem);
}

static void terminal_scroll(terminal_t* term, element_t* elem)
{
    term->cursorPos.y -= 1;

    rect_t rect;
    element_content_rect(elem, &rect);
    RECT_SHRINK(&rect, windowTheme.edgeWidth);

    // gfx_scroll(gfx, &rect, win_font(term->win)->height, windowTheme.dark);
}

static void terminal_draw_char(terminal_t* term, element_t* elem, const point_t* point, char chr)
{
    point_t clientPos = CURSOR_POS_TO_CLIENT_POS(point, term->font);
    element_draw_string(elem, term->font, &clientPos, windowTheme.bright, windowTheme.dark, &chr, 1);
}

static void terminal_put(terminal_t* term, element_t* elem, char chr)
{
    rect_t rect;
    element_content_rect(elem, &rect);

    switch (chr)
    {
    case '\n':
    {
        if (term->cursorVisible)
        {
            term->cursorVisible = false;
            terminal_cursor_draw(term, elem);
        }

        term->cursorPos.x = 0;
        term->cursorPos.y++;

        if (CURSOR_POS_Y_OUT_OF_BOUNDS(term->cursorPos.y + 1, term->font))
        {
            terminal_scroll(term, elem);
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
        terminal_draw_char(term, elem, &term->cursorPos, chr);
        term->cursorPos.x++;
        term->cursorVisible = false;

        if (CURSOR_POS_X_OUT_OF_BOUNDS(term->cursorPos.x + 1, term->font))
        {
            terminal_put(term, elem, '\n');
        }
    }
    break;
    }
}

static void terminal_write(terminal_t* term, element_t* elem, const char* str)
{
    const char* chr = str;
    while (*chr != '\0')
    {
        terminal_put(term, elem, *chr);
        chr++;
    }

    term->cursorVisible = true;
    terminal_cursor_draw(term, elem);
}

static void terminal_redraw_input(terminal_t* term, element_t* elem, uint64_t prevLength)
{
    point_t point = term->cursorPos;
    for (uint64_t i = 0; i < term->input.length; i++)
    {
        terminal_draw_char(term, elem, &point, term->input.buffer[i]);
        point.x++;
    }
    for (uint64_t i = term->input.length; i < prevLength + 1; i++)
    {
        terminal_draw_char(term, elem, &point, ' ');
        point.x++;
    }

    term->cursorVisible = true;
    terminal_cursor_draw(term, elem);
}

static void terminal_handle_input(terminal_t* term, element_t* elem, keycode_t key, char ascii, kbd_mods_t mods)
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
            terminal_redraw_input(term, elem, prevLength);
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
        terminal_redraw_input(term, elem, prevLength);
    }
    break;
    case KEY_LEFT:
    {
        if (input_move(&term->input, -1) != ERR)
        {
            terminal_redraw_input(term, elem, prevLength);
        }
    }
    break;
    case KEY_RIGHT:
    {
        if (input_move(&term->input, +1) != ERR)
        {
            terminal_redraw_input(term, elem, prevLength);
        }
    }
    break;
    case KEY_ENTER:
    {
        writef(term->stdin[PIPE_WRITE], "%s\n", term->input.buffer);

        term->cursorVisible = false;
        terminal_cursor_draw(term, elem);

        history_push(&term->history, term->input.buffer);
        input_set(&term->input, "");
        terminal_write(term, elem, "\n");
    }
    break;
    case KEY_BACKSPACE:
    {
        input_backspace(&term->input);
        terminal_redraw_input(term, elem, prevLength);
    }
    break;
    default:
    {
        if (ascii == '\0')
        {
            break;
        }

        if (CURSOR_POS_X_OUT_OF_BOUNDS(term->cursorPos.x + (int64_t)term->input.length + 2, term->font))
        {
            break;
        }

        input_insert(&term->input, ascii);
        terminal_redraw_input(term, elem, prevLength);
    }
    break;
    }
}

static uint64_t terminal_procedure(window_t* win, element_t* elem, const event_t* event)
{
    terminal_t* term = element_private(elem);

    switch (event->type)
    {
    case LEVENT_REDRAW:
    {
        terminal_clear(term, elem);
    }
    break;
    case UEVENT_TERMINAL_TIMER:
    {
        term->cursorVisible = !term->cursorVisible;
        terminal_cursor_draw(term, elem);
    }
    break;
    case EVENT_KBD:
    {
        if (event->kbd.type != KBD_PRESS || event->kbd.code == 0)
        {
            break;
        }
        terminal_handle_input(term, elem, event->kbd.code, event->kbd.ascii, event->kbd.mods);
    }
    break;
    }

    return 0;
}

void terminal_init(terminal_t* term)
{
    term->disp = display_new();

    rect_t rect = RECT_INIT_DIM(500, 200, TERMINAL_WIDTH, TERMINAL_HEIGHT);
    term->win = window_new(term->disp, "Terminal", &rect, SURFACE_WINDOW, WINDOW_DECO, terminal_procedure, term);
    if (term == NULL)
    {
        exit(errno);
        display_free(term->disp);
    }

    term->font = font_default(window_display(term->win));
    term->cursorPos = (point_t){0};
    term->cursorVisible = false;
    input_init(&term->input);
    history_init(&term->history);

    if (open2("sys:/pipe/new", term->stdin) == ERR || open2("sys:/pipe/new", term->stdout) == ERR)
    {
        window_free(term->win);
        display_free(term->disp);
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
        window_free(term->win);
        display_free(term->disp);
        exit(errno);
    }

    term->shellCtl = openf("sys:/proc/%d/ctl", shell);
    if (term->shellCtl == ERR)
    {
        close(term->stdin[0]);
        close(term->stdin[1]);
        close(term->stdout[0]);
        close(term->stdout[1]);
        window_free(term->win);
        display_free(term->disp);
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

    window_free(term->win);
    display_free(term->disp);
}

bool terminal_update(terminal_t* term)
{
    pollfd_t fds[] = {{.fd = term->stdout[PIPE_READ], .requested = POLL_READ},
        {.fd = display_fd(term->disp), .requested = POLL_READ}};
    if (poll(fds, 2, BLINK_INTERVAL) == 0)
    {
        display_emit(term->disp, window_id(term->win), UEVENT_TERMINAL_TIMER, NULL, 0);
    }

    bool shouldQuit = false;
    event_t event = {0};
    while (display_next_event(term->disp, &event, 0))
    {
        if (event.type == LEVENT_QUIT || !display_connected(term->disp))
        {
            shouldQuit = true;
        }
        display_dispatch(term->disp, &event);
    }

    if (fds[0].occurred & POLL_READ)
    {
        while (poll1(term->stdout[PIPE_READ], POLL_READ, 0) & POLL_READ)
        {
            char buffer[] = {0, '\0'};
            read(term->stdout[PIPE_READ], buffer, 1);
            terminal_write(term, window_client_element(term->win), buffer);
            input_set(&term->input, "");
        }

        display_cmds_flush(term->disp);
    }

    return !shouldQuit;
}

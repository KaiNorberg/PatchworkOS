#include "terminal.h"
#include "history.h"
#include "input.h"
#include "sys/io.h"
#include "sys/kbd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static void terminal_cursor_draw(terminal_t* term, element_t* elem, drawable_t* draw)
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
        draw_string(draw, term->font, &point, windowTheme.dark, windowTheme.bright, &cursor, 1);
    }
    else
    {
        draw_string(draw, term->font, &point, windowTheme.bright, windowTheme.dark, &cursor, 1);
    }
}

static void terminal_clear(terminal_t* term, element_t* elem, drawable_t* draw)
{
    term->cursorVisible = false;
    term->cursorPos = (point_t){0, 0};

    rect_t rect;
    element_content_rect(elem, &rect);

    draw_edge(draw, &rect, windowTheme.edgeWidth, windowTheme.shadow, windowTheme.highlight);
    RECT_SHRINK(&rect, windowTheme.edgeWidth);
    draw_rect(draw, &rect, windowTheme.dark);

    term->cursorVisible = true;
    terminal_cursor_draw(term, elem, draw);
}

static void terminal_scroll(terminal_t* term, element_t* elem, drawable_t* draw)
{
    term->cursorPos.y -= 1;

    uint64_t fontHeight = font_height(term->font);

    rect_t destRect;
    element_content_rect(elem, &destRect);
    RECT_SHRINK(&destRect, windowTheme.edgeWidth);
    RECT_SHRINK(&destRect, windowTheme.paddingWidth);
    destRect.bottom -= fontHeight;

    point_t srcPoint = {.x = destRect.left, .y = destRect.top + fontHeight};

    draw_transfer(draw, draw, &destRect, &srcPoint);

    rect_t bottomRect = RECT_INIT(destRect.left, destRect.bottom, destRect.right, destRect.bottom + fontHeight);
    draw_rect(draw, &bottomRect, windowTheme.dark);
}

static void terminal_draw_char(terminal_t* term, element_t* elem, drawable_t* draw, const point_t* point, char chr)
{
    point_t clientPos = CURSOR_POS_TO_CLIENT_POS(point, term->font);
    draw_string(draw, term->font, &clientPos, windowTheme.bright, windowTheme.dark, &chr, 1);
}

static void terminal_put(terminal_t* term, element_t* elem, drawable_t* draw, char chr)
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
            terminal_cursor_draw(term, elem, draw);
        }

        term->cursorPos.x = 0;
        term->cursorPos.y++;

        if (term->cursorPos.y >= TERMINAL_ROWS)
        {
            terminal_scroll(term, elem, draw);
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
        terminal_draw_char(term, elem, draw, &term->cursorPos, chr);
        term->cursorPos.x++;
        term->cursorVisible = false;

        if (term->cursorPos.x >= TERMINAL_COLUMNS)
        {
            terminal_put(term, elem, draw, '\n');
        }
    }
    break;
    }
}

static void terminal_write(terminal_t* term, element_t* elem, drawable_t* draw, const char* str)
{
    const char* chr = str;
    while (*chr != '\0')
    {
        terminal_put(term, elem, draw, *chr);
        chr++;
    }

    term->cursorVisible = true;
    terminal_cursor_draw(term, elem, draw);
    window_set_timer(term->win, TIMER_NONE, BLINK_INTERVAL);
}

static void terminal_redraw_input(terminal_t* term, element_t* elem, drawable_t* draw, uint64_t prevLength)
{
    point_t point = term->cursorPos;
    for (uint64_t i = 0; i < term->input.length; i++)
    {
        terminal_draw_char(term, elem, draw, &point, term->input.buffer[i]);
        point.x++;
    }
    for (uint64_t i = term->input.length; i < prevLength + 1; i++)
    {
        terminal_draw_char(term, elem, draw, &point, ' ');
        point.x++;
    }

    term->cursorVisible = true;
    terminal_cursor_draw(term, elem, draw);
    window_set_timer(term->win, TIMER_NONE, BLINK_INTERVAL);
}

static void terminal_handle_input(terminal_t* term, element_t* elem, drawable_t* draw, keycode_t key, char ascii,
    kbd_mods_t mods)
{
    uint64_t prevLength = term->input.length;

    switch (key)
    {
    case KBD_UP:
    {
        if (term->history.index == term->history.count)
        {
            input_save(&term->input);
        }

        const char* prev = history_previous(&term->history);
        if (prev != NULL)
        {
            input_set(&term->input, prev);
            terminal_redraw_input(term, elem, draw, prevLength);
        }
    }
    break;
    case KBD_DOWN:
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
        terminal_redraw_input(term, elem, draw, prevLength);
    }
    break;
    case KBD_LEFT:
    {
        if (input_move(&term->input, -1) != ERR)
        {
            terminal_redraw_input(term, elem, draw, prevLength);
        }
    }
    break;
    case KBD_RIGHT:
    {
        if (input_move(&term->input, +1) != ERR)
        {
            terminal_redraw_input(term, elem, draw, prevLength);
        }
    }
    break;
    case KBD_ENTER:
    {
        writef(term->stdin[PIPE_WRITE], "%s\n", term->input.buffer);

        term->cursorVisible = false;
        terminal_cursor_draw(term, elem, draw);

        history_push(&term->history, term->input.buffer);
        input_set(&term->input, "");
        terminal_write(term, elem, draw, "\n");
    }
    break;
    case KBD_BACKSPACE:
    {
        input_backspace(&term->input);
        terminal_redraw_input(term, elem, draw, prevLength);
    }
    break;
    default:
    {
        if (ascii == '\0')
        {
            break;
        }

        if (term->cursorPos.x + (int64_t)term->input.length + 1 >= TERMINAL_COLUMNS)
        {
            break;
        }

        input_insert(&term->input, ascii);
        terminal_redraw_input(term, elem, draw, prevLength);
    }
    break;
    }
}

static uint64_t terminal_procedure(window_t* win, element_t* elem, const event_t* event)
{
    terminal_t* term = element_private(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        window_set_timer(win, TIMER_NONE, BLINK_INTERVAL);
    }
    break;
    case LEVENT_REDRAW:
    {
        terminal_clear(term, elem, element_draw(elem));
    }
    break;
    case EVENT_TIMER:
    {
        window_set_timer(win, TIMER_NONE, BLINK_INTERVAL);

        term->cursorVisible = !term->cursorVisible;
        terminal_cursor_draw(term, elem, element_draw(elem));
    }
    break;
    case EVENT_KBD:
    {
        if (event->kbd.type != KBD_PRESS || event->kbd.code == 0)
        {
            break;
        }
        terminal_handle_input(term, elem, element_draw(elem), event->kbd.code, event->kbd.ascii, event->kbd.mods);
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

static void terminal_read_stdout(terminal_t* term)
{
    char buffer[MAX_PATH];

    do
    {
        uint64_t readCount = read(term->stdout[PIPE_READ], buffer, MAX_PATH - 1);
        buffer[readCount] = '\0';

        element_t* elem = window_client_element(term->win);

        terminal_write(term, elem, element_draw(elem), buffer);
        input_set(&term->input, "");
    } while (poll1(term->stdout[PIPE_READ], POLL_READ, 0) & POLL_READ);
}

bool terminal_update(terminal_t* term)
{
    pollfd_t fds[] = {{.fd = term->stdout[PIPE_READ], .requested = POLL_READ},
        {.fd = display_fd(term->disp), .requested = POLL_READ}};
    poll(fds, 2, CLOCKS_NEVER);

    event_t event = {0};
    while (display_next_event(term->disp, &event, 0) && event.type != LEVENT_QUIT && display_connected(term->disp))
    {
        display_dispatch(term->disp, &event);
    }

    if (event.type == LEVENT_QUIT || !display_connected(term->disp))
    {
        return false;
    }

    if (fds[0].occurred & POLL_READ)
    {
        terminal_read_stdout(term);
        display_cmds_flush(term->disp);
    }

    return true;
}

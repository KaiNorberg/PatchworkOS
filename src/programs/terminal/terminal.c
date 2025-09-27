#include "terminal.h"
#include "history.h"
#include "input.h"
#include "sys/io.h"
#include "sys/kbd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t terminal_width(terminal_t* term)
{
    const theme_t* theme = theme_global_get();
    return TERMINAL_COLUMNS * font_width(term->font, "a", 1) + theme->frameSize * 2 + theme->smallPadding * 2;
}

static uint64_t terminal_height(terminal_t* term)
{
    const theme_t* theme = theme_global_get();
    return TERMINAL_ROWS * font_height(term->font) + theme->frameSize * 2 + theme->smallPadding * 2;
}

static point_t terminal_cursor_pos_to_client_pos(terminal_t* term, element_t* elem, point_t* cursorPos)
{
    const theme_t* theme = element_get_theme(elem);
    return (point_t){
        .x = ((cursorPos)->x * font_width(term->font, "a", 1)) + theme->frameSize + theme->smallPadding,
        .y = ((cursorPos)->y * font_height(term->font)) + theme->frameSize + theme->smallPadding,
    };
}

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
    point_t point = terminal_cursor_pos_to_client_pos(term, elem, &cursorPos);

    rect_t cursorRect = RECT_INIT_DIM(point.x, point.y, font_width(term->font, &cursor, 1), font_height(term->font));

    if (cursor != ' ')
    {
        if (term->isCursorVisible)
        {
            draw_rect(draw, &cursorRect, TERMINAL_FOREGROUND);
            draw_string(draw, term->font, &point, TERMINAL_BACKGROUND, &cursor, 1);
        }
        else
        {
            draw_rect(draw, &cursorRect, TERMINAL_BACKGROUND);
            draw_string(draw, term->font, &point, TERMINAL_FOREGROUND, &cursor, 1);
        }
    }
    else
    {
        if (term->isCursorVisible)
        {
            draw_rect(draw, &cursorRect, TERMINAL_FOREGROUND);
        }
        else
        {
            draw_rect(draw, &cursorRect, TERMINAL_BACKGROUND);
        }
    }
}

static void terminal_clear(terminal_t* term, element_t* elem, drawable_t* draw)
{
    term->isCursorVisible = false;
    term->cursorPos = (point_t){0, 0};

    rect_t rect = element_get_content_rect(elem);

    const theme_t* theme = element_get_theme(elem);

    draw_frame(draw, &rect, theme->frameSize, theme->element.shadow, theme->element.highlight);
    RECT_SHRINK(&rect, theme->frameSize);
    draw_rect(draw, &rect, TERMINAL_BACKGROUND);

    term->isCursorVisible = true;
    terminal_cursor_draw(term, elem, draw);
}

static void terminal_scroll(terminal_t* term, element_t* elem, drawable_t* draw)
{
    term->cursorPos.y -= 1;

    uint64_t fontHeight = font_height(term->font);
    const theme_t* theme = element_get_theme(elem);

    rect_t destRect = element_get_content_rect(elem);

    RECT_SHRINK(&destRect, theme->frameSize);
    RECT_SHRINK(&destRect, theme->smallPadding);
    destRect.bottom -= fontHeight;

    point_t srcPoint = {.x = destRect.left, .y = destRect.top + fontHeight};
    draw_transfer(draw, draw, &destRect, &srcPoint);

    rect_t bottomRect = RECT_INIT(destRect.left, destRect.bottom, destRect.right, destRect.bottom + fontHeight);
    draw_rect(draw, &bottomRect, TERMINAL_BACKGROUND);
}

static void terminal_put(terminal_t* term, element_t* elem, drawable_t* draw, char chr)
{
    rect_t rect = element_get_content_rect(elem);

    switch (chr)
    {
    case '\n':
    {
        if (term->isCursorVisible)
        {
            term->isCursorVisible = false;
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
        term->isCursorVisible = false;
        terminal_cursor_draw(term, elem, draw);

        point_t clientPos = terminal_cursor_pos_to_client_pos(term, elem, &term->cursorPos);
        draw_string(draw, term->font, &clientPos, TERMINAL_FOREGROUND, &chr, 1);

        term->cursorPos.x++;

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

    term->isCursorVisible = true;
    terminal_cursor_draw(term, elem, draw);
    window_set_timer(term->win, TIMER_NONE, BLINK_INTERVAL);
}

static void terminal_redraw_input(terminal_t* term, element_t* elem, drawable_t* draw, uint64_t prevLength)
{
    // Clear the previous line
    point_t point = terminal_cursor_pos_to_client_pos(term, elem, &term->cursorPos);
    rect_t previousRect =
        RECT_INIT_DIM(point.x, point.y, (prevLength + 1) * font_width(term->font, "a", 1), font_height(term->font));
    draw_rect(draw, &previousRect, TERMINAL_BACKGROUND);

    draw_string(draw, term->font, &point, TERMINAL_FOREGROUND, term->input.buffer, strlen(term->input.buffer));

    term->isCursorVisible = true;
    terminal_cursor_draw(term, elem, draw);
    window_set_timer(term->win, TIMER_NONE, BLINK_INTERVAL);
}

static void terminal_handle_input(terminal_t* term, element_t* elem, drawable_t* draw, keycode_t key, char ascii,
    kbd_mods_t mods)
{
    (void)mods; // Currently unused

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

        term->isCursorVisible = false;
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
    terminal_t* term = element_get_private(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        window_set_timer(win, TIMER_NONE, BLINK_INTERVAL);
    }
    break;
    case LEVENT_REDRAW:
    {
        drawable_t draw;
        element_draw_begin(elem, &draw);

        terminal_clear(term, elem, &draw);

        element_draw_end(elem, &draw);
    }
    break;
    case EVENT_TIMER:
    {
        window_set_timer(win, TIMER_NONE, BLINK_INTERVAL);

        term->isCursorVisible = !term->isCursorVisible;

        drawable_t draw;
        element_draw_begin(elem, &draw);

        terminal_cursor_draw(term, elem, &draw);

        element_draw_end(elem, &draw);
    }
    break;
    case EVENT_KBD:
    {
        if (event->kbd.type != KBD_PRESS || event->kbd.code == 0)
        {
            break;
        }

        drawable_t draw;
        element_draw_begin(elem, &draw);

        terminal_handle_input(term, elem, &draw, event->kbd.code, event->kbd.ascii, event->kbd.mods);

        element_draw_end(elem, &draw);
    }
    break;
    }

    return 0;
}

void terminal_init(terminal_t* term)
{
    term->disp = display_new();
    term->font = font_new(term->disp, "firacode", "retina", 16);

    rect_t rect = RECT_INIT_DIM(500, 200, terminal_width(term), terminal_height(term));
    term->win = window_new(term->disp, "Terminal", &rect, SURFACE_WINDOW, WINDOW_DECO, terminal_procedure, term);
    if (term == NULL)
    {
        exit(errno);
        display_free(term->disp);
    }

    term->cursorPos = (point_t){0};
    term->isCursorVisible = false;
    input_init(&term->input);
    history_init(&term->history);

    if (open2("/dev/pipe", term->stdin) == ERR || open2("/dev/pipe", term->stdout) == ERR)
    {
        window_free(term->win);
        display_free(term->disp);
        exit(errno);
    }

    const char* argv[] = {"/bin/shell", NULL};
    spawn_fd_t fds[] = {
        {.child = STDIN_FILENO, .parent = term->stdin[PIPE_READ]},
        {.child = STDOUT_FILENO, .parent = term->stdout[PIPE_WRITE]},
        {.child = STDERR_FILENO, .parent = term->stdout[PIPE_WRITE]},
        SPAWN_FD_END,
    };
    term->shell = spawn(argv, fds, NULL, NULL);
    if (term->shell == ERR)
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

    fd_t shellNote = openf("/proc/%d/note", term->shell);
    writef(shellNote, "kill");
    close(shellNote);

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

        element_t* elem = window_get_client_element(term->win);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        terminal_write(term, elem, &draw, buffer);

        element_draw_end(elem, &draw);
        window_invalidate_flush(term->win);

        input_set(&term->input, "");
    } while (poll1(term->stdout[PIPE_READ], POLLIN, 0) & POLLIN);
}

bool terminal_update(terminal_t* term)
{
    pollfd_t fds[] = {{.fd = term->stdout[PIPE_READ], .events = POLLIN},
        {.fd = display_fd(term->disp), .events = POLLIN}};
    poll(fds, 2, CLOCKS_NEVER);

    event_t event = {0};
    while (display_next_event(term->disp, &event, 0) && event.type != LEVENT_QUIT && display_is_connected(term->disp))
    {
        display_dispatch(term->disp, &event);
    }

    if (event.type == LEVENT_QUIT || !display_is_connected(term->disp))
    {
        return false;
    }

    if (fds[0].revents & POLLIN)
    {
        terminal_read_stdout(term);
        display_cmds_flush(term->disp);
    }

    return true;
}

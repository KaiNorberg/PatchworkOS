#include "terminal.h"
#include "ansi.h"

#include <libpatchwork/display.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/defs.h>
#include <sys/io.h>
#include <sys/kbd.h>
#include <sys/proc.h>
#include <time.h>

static terminal_char_t terminal_char_create(char chr, pixel_t foreground, pixel_t background, uint16_t row,
    uint16_t col)
{
    terminal_char_t termChar;
    termChar.chr = chr;
    termChar.foreground = foreground;
    termChar.background = background;
    termChar.flags = 0;
    termChar.physicalRow = row;
    termChar.col = col;
    return termChar;
}

static terminal_char_t* terminal_get_char(terminal_t* term, uint16_t row, uint16_t col)
{
    return &term->screen[(term->firstRow + row) % TERMINAL_ROWS][col];
}

static uint16_t terminal_char_row(terminal_t* term, terminal_char_t* termChar)
{
    return (termChar->physicalRow + TERMINAL_ROWS - term->firstRow) % TERMINAL_ROWS;
}

static point_t terminal_char_pos(terminal_t* term, element_t* elem, terminal_char_t* termChar)
{
    const theme_t* theme = element_get_theme(elem);

    uint16_t row = terminal_char_row(term, termChar);
    uint16_t col = termChar->col;

    return (point_t){
        .x = (col * font_width(term->font, "a", 1)) + theme->frameSize + theme->bigPadding,
        .y = (row * font_height(term->font)) + theme->frameSize + theme->bigPadding,
    };
}

static rect_t terminal_char_rect(terminal_t* term, element_t* elem, terminal_char_t* termChar)
{
    point_t clientPos = terminal_char_pos(term, elem, termChar);
    return RECT_INIT_DIM(clientPos.x, clientPos.y, font_width(term->font, "a", 1), font_height(term->font));
}

static void terminal_char_draw(terminal_t* term, element_t* elem, drawable_t* draw, terminal_char_t* termChar)
{
    point_t clientPos = terminal_char_pos(term, elem, termChar);
    rect_t charRect = terminal_char_rect(term, elem, termChar);
    draw_rect(draw, &charRect, termChar->flags & TERMINAL_INVERSE ? termChar->foreground : termChar->background);
    draw_string(draw, term->font, &clientPos,
        termChar->flags & TERMINAL_INVERSE ? termChar->background : termChar->foreground, &termChar->chr, 1);

    if (termChar->flags & TERMINAL_UNDERLINE)
    {
        rect_t underlineRect = RECT_INIT_DIM(charRect.left, charRect.bottom - 1, RECT_WIDTH(&charRect), 1);
        draw_rect(draw, &underlineRect,
            termChar->flags & TERMINAL_INVERSE ? termChar->background : termChar->foreground);
    }
}

static void terminal_cursor_update(terminal_t* term, element_t* elem, drawable_t* draw)
{
    term->prevCursor->flags &= ~TERMINAL_INVERSE;
    terminal_char_draw(term, elem, draw, term->prevCursor);
    if (term->isCursorVisible)
    {
        if (term->cursorBlink)
        {
            term->cursor->flags |= TERMINAL_INVERSE;
        }
        terminal_char_draw(term, elem, draw, term->cursor);
    }
    term->prevCursor = term->cursor;
}

static void terminal_clear(terminal_t* term, element_t* elem, drawable_t* draw)
{
    rect_t rect = element_get_content_rect(elem);
    const theme_t* theme = element_get_theme(elem);

    RECT_SHRINK(&rect, theme->frameSize);
    RECT_SHRINK(&rect, theme->smallPadding);

    draw_rect(draw, &rect, term->background);

    term->cursor = &term->screen[0][0];
    term->prevCursor = &term->screen[0][0];
}

static void terminal_scroll(terminal_t* term, element_t* elem, drawable_t* draw)
{
    const theme_t* theme = element_get_theme(elem);

    term->prevCursor->flags &= ~TERMINAL_INVERSE;
    terminal_char_draw(term, elem, draw, term->prevCursor);

    for (uint64_t col = 0; col < TERMINAL_COLUMNS; col++)
    {
        term->screen[term->firstRow][col] =
            terminal_char_create(' ', theme->ansi.bright[7], theme->ansi.normal[0], term->firstRow, col);
    }
    term->firstRow = (term->firstRow + 1) % TERMINAL_ROWS;

    rect_t contentRect = element_get_content_rect(elem);
    RECT_SHRINK(&contentRect, theme->frameSize);
    RECT_SHRINK(&contentRect, theme->bigPadding);

    uint64_t rowHeight = font_height(term->font);

    rect_t destRect = RECT_INIT_DIM(contentRect.left, contentRect.top, RECT_WIDTH(&contentRect),
        RECT_HEIGHT(&contentRect) - rowHeight);
    point_t srcPoint = {contentRect.left, contentRect.top + rowHeight};

    draw_transfer(draw, draw, &destRect, &srcPoint);

    rect_t clearRect =
        RECT_INIT_DIM(contentRect.left, contentRect.bottom - rowHeight, RECT_WIDTH(&contentRect), rowHeight);
    draw_rect(draw, &clearRect, term->background);

    term->cursor = terminal_get_char(term, TERMINAL_ROWS - 1, 0);
    term->prevCursor = term->cursor;
}

static void terminal_put(terminal_t* term, element_t* elem, drawable_t* draw, char chr)
{
    rect_t rect = element_get_content_rect(elem);

    uint16_t cursorRow = terminal_char_row(term, term->cursor);
    switch (chr)
    {
    case '\n':
        if (terminal_char_row(term, term->cursor) == TERMINAL_ROWS - 1)
        {
            terminal_scroll(term, elem, draw);
        }
        else
        {
            term->cursor = terminal_get_char(term, cursorRow + 1, 0);
        }
        break;
    case '\r':
        term->cursor = terminal_get_char(term, cursorRow, 0);
        break;
    case '\b':
    {
        if (term->cursor->col == 0)
        {
            if (cursorRow == 0)
            {
                break;
            }
            term->cursor = terminal_get_char(term, cursorRow - 1, TERMINAL_COLUMNS - 1);
        }
        else
        {
            term->cursor = terminal_get_char(term, cursorRow, term->cursor->col - 1);
        }
        terminal_char_t* backspaceChar = term->cursor;
        backspaceChar->chr = ' ';
        backspaceChar->foreground = term->foreground;
        backspaceChar->background = term->background;
        backspaceChar->flags = term->flags;
        terminal_char_draw(term, elem, draw, backspaceChar);
    }
    break;
    case '\t':
    {
        uint16_t spacesToNextTabStop = 4 - (term->cursor->col % 4);
        for (uint16_t i = 0; i < spacesToNextTabStop; i++)
        {
            terminal_put(term, elem, draw, ' ');
        }
    }
    break;
    default:
    {
        terminal_char_t* currentChar = term->cursor;
        currentChar->chr = chr;
        currentChar->foreground = term->foreground;
        currentChar->background = term->background;
        currentChar->flags = term->flags;
        terminal_char_draw(term, elem, draw, currentChar);

        uint16_t cursorRow = terminal_char_row(term, term->cursor);
        if (term->cursor->col == TERMINAL_COLUMNS - 1)
        {
            if (cursorRow == TERMINAL_ROWS - 1)
            {
                terminal_scroll(term, elem, draw);
            }
            else
            {
                term->cursor = terminal_get_char(term, cursorRow + 1, 0);
            }
        }
        else
        {
            term->cursor = terminal_get_char(term, cursorRow, term->cursor->col + 1);
        }
    }
    break;
    }
}

static void terminal_handle_input(terminal_t* term, element_t* elem, drawable_t* draw, const event_kbd_t* kbd)
{
    UNUSED(elem);
    UNUSED(draw);

    ansi_receiving_t ansi;
    ansi_kbd_to_receiving(&ansi, kbd);

    if (ansi.length > 0)
    {
        write(term->stdin[PIPE_WRITE], ansi.buffer, ansi.length);
    }

    if (ansi.length == 1 && ansi.buffer[0] == '\003')
    {
        swritefile(F("/proc/%llu/notegroup", term->shell), "interrupt due to ctrl+c");
    }
}

static void terminal_execute_ansi(terminal_t* term, element_t* elem, drawable_t* draw, ansi_sending_t* ansi)
{
    if (ansi->ascii)
    {
        terminal_put(term, elem, draw, ansi->command);
        return;
    }

    if (ansi->extended)
    {
        switch (ansi->command)
        {
        case 'h': // Set Mode
            if (ansi->parameters[0] == 25)
            {
                term->isCursorVisible = true;
                terminal_cursor_update(term, elem, draw);
            }
            break;
        case 'l': // Reset Mode
            if (ansi->parameters[0] == 25)
            {
                term->isCursorVisible = false;
                terminal_cursor_update(term, elem, draw);
            }
            break;
        default:
            goto not_handled;
        }

        return;
    }

    switch (ansi->command)
    {
    case 'H': // Cursor to (0, 0) or specified position
        if (ansi->paramCount >= 2)
        {
            uint16_t row = ansi->parameters[0] == 0 ? 0 : ansi->parameters[0] - 1;
            uint16_t col = ansi->parameters[1] == 0 ? 0 : ansi->parameters[1] - 1;
            row = MIN(row, TERMINAL_ROWS - 1);
            col = MIN(col, TERMINAL_COLUMNS - 1);
            term->cursor = terminal_get_char(term, row, col);
            break;
        }
        term->cursor = terminal_get_char(term, 0, 0);
        break;
    case 'A': // Cursor Up
    {
        uint16_t cursorRow = terminal_char_row(term, term->cursor);
        uint16_t moveBy = ansi->parameters[0] == 0 ? 1 : ansi->parameters[0];
        uint16_t startPos = cursorRow < moveBy ? 0 : cursorRow - moveBy;
        term->cursor = terminal_get_char(term, startPos, term->cursor->col);
    }
    break;
    case 'B': // Cursor Down
    {
        uint16_t cursorRow = terminal_char_row(term, term->cursor);
        uint16_t moveBy = ansi->parameters[0] == 0 ? 1 : ansi->parameters[0];
        uint16_t endPos = MIN(TERMINAL_ROWS - 1, cursorRow + moveBy);
        term->cursor = terminal_get_char(term, endPos, term->cursor->col);
    }
    break;
    case 'C': // Cursor Forward
    {
        uint16_t moveBy = ansi->parameters[0] == 0 ? 1 : ansi->parameters[0];
        uint16_t endPos = MIN(TERMINAL_COLUMNS - 1, term->cursor->col + moveBy);
        term->cursor = terminal_get_char(term, terminal_char_row(term, term->cursor), endPos);
    }
    break;
    case 'D': // Cursor Backward
    {
        uint16_t moveBy = ansi->parameters[0] == 0 ? 1 : ansi->parameters[0];
        uint16_t startPos = term->cursor->col < moveBy ? 0 : term->cursor->col - moveBy;
        term->cursor = terminal_get_char(term, terminal_char_row(term, term->cursor), startPos);
    }
    break;
    case 'n':
    {
        if (ansi->parameters[0] != 6)
        {
            break;
        }

        // Report Cursor Position
        uint16_t cursorRow = terminal_char_row(term, term->cursor) + 1;
        uint16_t cursorCol = term->cursor->col + 1;
        char response[MAX_NAME];
        int responseLen = snprintf(response, sizeof(response), "\033[%d;%dR", cursorRow, cursorCol);
        write(term->stdin[PIPE_WRITE], response, responseLen);
    }
    break;
    case 's': // Save Cursor Position
        term->savedCursor = term->cursor;
        break;
    case 'u': // Restore Cursor Position
        terminal_cursor_update(term, elem, draw);
        term->prevCursor = term->cursor;
        term->cursor = term->savedCursor;
        break;
    case 'J': // Erase in Screen
    {
        uint16_t cursorRow = terminal_char_row(term, term->cursor);
        uint16_t cursorCol = term->cursor->col;
        uint16_t startRow = 0;
        uint16_t startCol = 0;
        uint16_t endRow = 0;
        uint16_t endCol = 0;
        switch (ansi->parameters[0])
        {
        case 0: // From cursor to end of screen
            startRow = cursorRow;
            startCol = cursorCol;
            endRow = TERMINAL_ROWS - 1;
            endCol = TERMINAL_COLUMNS - 1;
            break;
        case 1: // From beginning of screen to cursor
            startRow = 0;
            startCol = 0;
            endRow = cursorRow;
            endCol = cursorCol;
            break;
        case 2: // Entire screen
            startRow = 0;
            startCol = 0;
            endRow = TERMINAL_ROWS - 1;
            endCol = TERMINAL_COLUMNS - 1;
            break;
        default:
            break;
        }

        for (uint16_t row = startRow; row <= endRow; row++)
        {
            uint16_t colStart = (row == startRow) ? startCol : 0;
            uint16_t colEnd = (row == endRow) ? endCol : TERMINAL_COLUMNS - 1;
            for (uint16_t col = colStart; col <= colEnd; col++)
            {
                terminal_char_t* screenChar = terminal_get_char(term, row, col);
                screenChar->chr = ' ';
                screenChar->foreground = term->foreground;
                screenChar->background = term->background;
                screenChar->flags = term->flags;
                terminal_char_draw(term, elem, draw, screenChar);
            }
        }
    }
    break;
    case 'K': // Erase in Line
    {
        uint16_t cursorRow = terminal_char_row(term, term->cursor);
        uint16_t startCol = 0;
        uint16_t endCol = 0;
        switch (ansi->parameters[0])
        {
        case 0: // From cursor to end of line
            startCol = term->cursor->col;
            endCol = TERMINAL_COLUMNS - 1;
            break;
        case 1: // From beginning of line to cursor
            startCol = 0;
            endCol = term->cursor->col;
            break;
        case 2: // Entire line
            startCol = 0;
            endCol = TERMINAL_COLUMNS - 1;
            break;
        default:
            break;
        }

        for (uint16_t col = startCol; col <= endCol; col++)
        {
            terminal_char_t* lineChar = terminal_get_char(term, cursorRow, col);
            lineChar->chr = ' ';
            lineChar->foreground = term->foreground;
            lineChar->background = term->background;
            lineChar->flags = term->flags;
            terminal_char_draw(term, elem, draw, lineChar);
        }
    }
    break;
    case 'm': // Graphic Rendition
    {
        if (ansi->paramCount != 1)
        {
            /// @todo Implement support for more advanced color stuff
            return;
        }

        const theme_t* theme = element_get_theme(elem);
        switch (ansi->parameters[0])
        {
        case 0:
            term->foreground = theme->ansi.bright[7];
            term->background = theme->ansi.normal[0];
            term->flags = 0;
            break;
        case 1:
            term->flags |= TERMINAL_BOLD;
            break;
        case 2:
            term->flags |= TERMINAL_DIM;
            break;
        case 3:
            term->flags |= TERMINAL_ITALIC;
            break;
        case 4:
            term->flags |= TERMINAL_UNDERLINE;
            break;
        case 5:
            term->flags |= TERMINAL_BLINK;
            break;
        case 6:
            term->flags |= TERMINAL_INVERSE;
            break;
        case 7:
            term->flags |= TERMINAL_HIDDEN;
            break;
        case 8:
            term->flags |= TERMINAL_STRIKETHROUGH;
            break;
        case 9:
            term->flags |= TERMINAL_STRIKETHROUGH;
            break;
        case 22:
            term->flags &= ~(TERMINAL_BOLD | TERMINAL_DIM);
            break;
        case 23:
            term->flags &= ~TERMINAL_ITALIC;
            break;
        case 24:
            term->flags &= ~TERMINAL_UNDERLINE;
            break;
        case 25:
            term->flags &= ~TERMINAL_BLINK;
            break;
        case 27:
            term->flags &= ~TERMINAL_INVERSE;
            break;
        case 28:
            term->flags &= ~TERMINAL_HIDDEN;
            break;
        case 29:
            term->flags &= ~TERMINAL_STRIKETHROUGH;
            break;
        default:
            if (ansi->parameters[0] >= 30 && ansi->parameters[0] <= 37)
            {
                term->foreground = theme->ansi.normal[ansi->parameters[0] - 30];
            }
            else if (ansi->parameters[0] == 39)
            {
                term->foreground = theme->ansi.bright[7];
            }
            else if (ansi->parameters[0] >= 90 && ansi->parameters[0] <= 97)
            {
                term->foreground = theme->ansi.bright[ansi->parameters[0] - 90];
            }
            else if (ansi->parameters[0] >= 40 && ansi->parameters[0] <= 47)
            {
                term->background = theme->ansi.normal[ansi->parameters[0] - 40];
            }
            else if (ansi->parameters[0] == 49)
            {
                term->background = theme->ansi.normal[0];
            }
            else if (ansi->parameters[0] >= 100 && ansi->parameters[0] <= 107)
            {
                term->background = theme->ansi.bright[ansi->parameters[0] - 100];
            }
            break;
        }
    }
    break;
    }

    return;
not_handled:
    terminal_put(term, elem, draw, '\033');
    terminal_put(term, elem, draw, '[');
    for (uint64_t i = 0; i < ansi->paramCount; i++)
    {
        if (i > 0)
        {
            terminal_put(term, elem, draw, ';');
        }
        char paramStr[MAX_NAME];
        int paramLen = snprintf(paramStr, sizeof(paramStr), "%d", ansi->parameters[i]);
        for (int j = 0; j < paramLen; j++)
        {
            terminal_put(term, elem, draw, paramStr[j]);
        }
    }
    terminal_put(term, elem, draw, ansi->command);
}

static void terminal_handle_output(terminal_t* term, element_t* elem, drawable_t* draw, const char* buffer,
    uint64_t length)
{
    for (uint64_t i = 0; i < length; i++)
    {
        if (ansi_sending_parse(&term->ansi, buffer[i]))
        {
            terminal_execute_ansi(term, elem, draw, &term->ansi);
        }
    }

    term->cursorBlink = true;
    terminal_cursor_update(term, elem, draw);
    window_set_timer(term->win, TIMER_NONE, TERMINAL_BLINK_INTERVAL);
}

static uint64_t terminal_procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case EVENT_LIB_INIT:
    {
        terminal_init_ctx_t* ctx = element_get_private(elem);

        terminal_t* term = malloc(sizeof(terminal_t));
        if (term == NULL)
        {
            return ERR;
        }
        term->win = win;
        term->font = ctx->font;
        term->cursorBlink = false;
        term->isCursorVisible = true;

        if (open2("/dev/pipe/new", term->stdin) == ERR)
        {
            font_free(term->font);
            free(term);
            return ERR;
        }
        if (open2("/dev/pipe/new", term->stdout) == ERR)
        {
            close(term->stdin[0]);
            close(term->stdin[1]);
            font_free(term->font);
            free(term);
            return ERR;
        }

        const theme_t* theme = element_get_theme(elem);
        term->foreground = theme->ansi.bright[7];
        term->background = theme->ansi.normal[0];
        term->flags = 0;
        ansi_sending_init(&term->ansi);
        for (uint64_t row = 0; row < TERMINAL_ROWS; row++)
        {
            for (uint64_t col = 0; col < TERMINAL_COLUMNS; col++)
            {
                term->screen[row][col] = terminal_char_create(' ', term->foreground, term->background, row, col);
            }
        }
        term->firstRow = 0;
        term->savedCursor = &term->screen[0][0];
        term->cursor = &term->screen[0][0];
        term->prevCursor = &term->screen[0][0];

        const char* argv[] = {"/base/bin/shell", NULL};
        term->shell = spawn(argv, SPAWN_SUSPEND | SPAWN_EMPTY_GROUP | SPAWN_COPY_NS);
        if (term->shell == ERR)
        {
            close(term->stdin[0]);
            close(term->stdin[1]);
            close(term->stdout[0]);
            close(term->stdout[1]);
            font_free(term->font);
            free(term);
            return ERR;
        }

        if (swritefile(F("/proc/%d/ctl", term->shell),
                F("dup2 %d 0 && dup2 %d 1 && dup2 %d 2 && close 3 -1 && start", term->stdin[0], term->stdout[1],
                    term->stdout[1])) == ERR)
        {
            swritefile(F("/proc/%d/ctl", term->shell), "kill");
            close(term->stdin[0]);
            close(term->stdin[1]);
            close(term->stdout[0]);
            close(term->stdout[1]);
            font_free(term->font);
            free(term);
            return ERR;
        }

        element_set_private(elem, term);
        window_set_timer(win, TIMER_NONE, TERMINAL_BLINK_INTERVAL);
    }
    break;
    case EVENT_LIB_DEINIT:
    {
        terminal_t* term = element_get_private(elem);
        if (term == NULL)
        {
            break;
        }

        close(term->stdin[0]);
        close(term->stdin[1]);
        close(term->stdout[0]);
        close(term->stdout[1]);

        swritefile(F("/proc/%d/notegroup", term->shell), "terminate due to terminal close");
    }
    break;
    case EVENT_LIB_QUIT:
    {
        display_disconnect(window_get_display(win));
    }
    break;
    case EVENT_LIB_REDRAW:
    {
        terminal_t* term = element_get_private(elem);

        drawable_t draw;
        element_draw_begin(elem, &draw);
        terminal_clear(term, elem, &draw);
        element_draw_end(elem, &draw);
    }
    break;
    case EVENT_TIMER:
    {
        terminal_t* term = element_get_private(elem);

        window_set_timer(win, TIMER_NONE, TERMINAL_BLINK_INTERVAL);

        term->cursorBlink = !term->cursorBlink;
        drawable_t draw;
        element_draw_begin(elem, &draw);
        terminal_cursor_update(term, elem, &draw);
        element_draw_end(elem, &draw);
    }
    break;
    case EVENT_KBD:
    {
        terminal_t* term = element_get_private(elem);

        if (event->kbd.type != KBD_PRESS || event->kbd.code == 0)
        {
            break;
        }

        drawable_t draw;
        element_draw_begin(elem, &draw);
        terminal_handle_input(term, elem, &draw, &event->kbd);
        element_draw_end(elem, &draw);
    }
    break;
    default:
        break;
    }

    return 0;
}

static uint64_t terminal_pixel_width(font_t* font)
{
    const theme_t* theme = theme_global_get();
    return TERMINAL_COLUMNS * font_width(font, "a", 1) + theme->frameSize * 2 + theme->bigPadding * 2;
}

static uint64_t terminal_pixel_height(font_t* font)
{
    const theme_t* theme = theme_global_get();
    return TERMINAL_ROWS * font_height(font) + theme->frameSize * 2 + theme->bigPadding * 2;
}

window_t* terminal_new(display_t* disp)
{
    terminal_init_ctx_t ctx = {
        .font = font_new(disp, "firacode", "retina", 16),
    };
    if (ctx.font == NULL)
    {
        return NULL;
    }

    rect_t rect = RECT_INIT_DIM(500, 200, terminal_pixel_width(ctx.font), terminal_pixel_height(ctx.font));
    window_t* win = window_new(disp, "Terminal", &rect, SURFACE_WINDOW, WINDOW_DECO, terminal_procedure, &ctx);
    if (win == NULL)
    {
        font_free(ctx.font);
        return NULL;
    }

    if (window_set_visible(win, true) == ERR)
    {
        window_free(win);
        font_free(ctx.font);
        return NULL;
    }

    return win;
}

void terminal_loop(window_t* win)
{
    terminal_t* terminal = element_get_private(window_get_client_element(win));
    display_t* disp = window_get_display(win);

    char buffer[TERMINAL_MAX_DATA] = {0};
    uint64_t length = 0;

    clock_t currentTime = clock();
    clock_t nextFrameTime = currentTime + (CLOCKS_PER_SEC / TERMINAL_MAX_FPS);
    while (true)
    {
        clock_t timeout;
        if (length == 0)
        {
            timeout = CLOCKS_NEVER;
        }
        else if (length == TERMINAL_MAX_DATA)
        {
            timeout = 0;
        }
        else
        {
            timeout = nextFrameTime - currentTime;
        }

        pollfd_t fds[1] = {{
            .fd = terminal->stdout[PIPE_READ],
            .events = POLLIN,
        }};
        if (display_poll(disp, fds, 1, timeout) == ERR)
        {
            break;
        }

        event_t event = {0};
        while (display_next(disp, &event, 0) != ERR)
        {
            display_dispatch(disp, &event);
        }

        if ((!(fds[0].revents & POLLIN) && length > 0) || length == TERMINAL_MAX_DATA)
        {
            element_t* elem = window_get_client_element(terminal->win);
            terminal_t* term = element_get_private(elem);

            drawable_t draw;
            element_draw_begin(elem, &draw);
            terminal_handle_output(term, elem, &draw, buffer, length);
            element_draw_end(elem, &draw);

            length = 0;
            window_invalidate_flush(terminal->win);
            display_cmds_flush(disp);
        }

        if (fds[0].revents & POLLIN)
        {
            uint64_t readCount = read(terminal->stdout[PIPE_READ], &buffer[length], TERMINAL_MAX_DATA - length);
            if (readCount == ERR || readCount == 0)
            {
                break;
            }
            length += readCount;
        }

        currentTime = clock();
        if (currentTime >= nextFrameTime)
        {
            nextFrameTime = MAX(nextFrameTime + (CLOCKS_PER_SEC / TERMINAL_MAX_FPS), currentTime + 1);
        }
    }
}

#include "screen.h"

#include "glyphs.h"

#include <sys/math.h>

static screen_line_t* screen_buffer_get_line(screen_buffer_t* buffer, uint64_t y)
{
    uint64_t index = (y + buffer->firstLineIndex) % buffer->height;
    return &buffer->lines[index];
}

static void screen_buffer_invalidate(screen_buffer_t* buffer, const screen_pos_t* pos)
{
    if (buffer->invalidStart.x == 0 && buffer->invalidStart.y == 0 && buffer->invalidEnd.x == 0 &&
        buffer->invalidEnd.y == 0)
    {
        buffer->invalidStart = *pos;
        buffer->invalidEnd = (screen_pos_t){pos->x + 1, pos->y + 1};
    }
    else
    {
        buffer->invalidStart.x = MIN(buffer->invalidStart.x, pos->x);
        buffer->invalidStart.y = MIN(buffer->invalidStart.y, pos->y);
        buffer->invalidEnd.x = MAX(buffer->invalidEnd.x, pos->x + 1);
        buffer->invalidEnd.y = MAX(buffer->invalidEnd.y, pos->y + 1);
    }

    buffer->invalidEnd.x = MIN(buffer->invalidEnd.x, buffer->width);
    buffer->invalidEnd.y = MIN(buffer->invalidEnd.y, buffer->height);
}

static void screen_buffer_put(screen_buffer_t* buffer, const screen_pos_t* pos, char chr)
{
    if (chr == '\n' || chr < ' ')
    {
        chr = ' ';
    }

    const glyph_cache_t* cache = glyph_cache_get();
    const glyph_t* glyph = &cache->glyphs[(uint8_t)chr];

    screen_line_t* line = screen_buffer_get_line(buffer, pos->y);

    uint64_t pixelX = pos->x * GLYPH_WIDTH;
    for (uint64_t y = 0; y < GLYPH_HEIGHT; y++)
    {
        memcpy(&line->pixels[pixelX + y * SCREEN_LINE_STRIDE], &glyph->pixels[y * GLYPH_WIDTH],
            sizeof(uint32_t) * GLYPH_WIDTH);
    }

    line->length = MAX(line->length, pos->x + 1);
    screen_buffer_invalidate(buffer, pos);
}

static void screen_buffer_flush(screen_buffer_t* buffer, boot_gop_t* gop)
{
    for (uint64_t y = buffer->invalidStart.y; y < buffer->invalidEnd.y; y++)
    {
        screen_line_t* line = screen_buffer_get_line(buffer, y);

        for (uint64_t pixelY = 0; pixelY < GLYPH_HEIGHT; pixelY++)
        {
            memcpy(&gop->virtAddr[buffer->invalidStart.x * GLYPH_WIDTH + (y * GLYPH_HEIGHT + pixelY) * gop->stride],
                &line->pixels[buffer->invalidStart.x * GLYPH_WIDTH + pixelY * SCREEN_LINE_STRIDE],
                (buffer->invalidEnd.x - buffer->invalidStart.x) * GLYPH_WIDTH * sizeof(uint32_t));
        }
    }

    buffer->invalidStart = (screen_pos_t){0, 0};
    buffer->invalidEnd = (screen_pos_t){0, 0};
}

static void screen_buffer_clear(screen_buffer_t* buffer)
{
    buffer->firstLineIndex = 0;
    buffer->invalidStart = (screen_pos_t){0, 0};
    buffer->invalidEnd = (screen_pos_t){0, 0};

    for (uint64_t i = 0; i < buffer->height; i++)
    {
        buffer->lines[i].length = 0;
        memset(buffer->lines[i].pixels, 0, sizeof(buffer->lines[i].pixels));
    }
}

static void screen_clear(screen_t* screen)
{
    screen_buffer_clear(&screen->buffer);

    for (uint64_t y = 0; y < screen->gop.height; y++)
    {
        memset32(&screen->gop.virtAddr[y * screen->gop.stride], 0xFF000000, screen->gop.width);
    }
}

void screen_init(screen_t* screen, const boot_gop_t* gop)
{
    screen->initialized = true;
    screen->gop = *gop;
    screen->cursor = (screen_pos_t){0, 0};

    screen->buffer.width = MIN(gop->width / GLYPH_WIDTH, SCREEN_LINE_MAX_LENGTH);
    screen->buffer.height = MIN(gop->height / GLYPH_HEIGHT, CONFIG_SCREEN_MAX_LINES);

    screen_clear(screen);
}

static void screen_scroll(screen_t* screen)
{
    uint64_t newCursorY = screen->cursor.y != 0 ? screen->cursor.y - 1 : 0;
    for (uint64_t y = 0; y < newCursorY; y++)
    {
        screen_line_t* line = screen_buffer_get_line(&screen->buffer, y);
        screen_line_t* newLine = screen_buffer_get_line(&screen->buffer, y + 1);

        for (uint64_t offsetY = 0; offsetY < GLYPH_HEIGHT; offsetY++)
        {
            memcpy(&screen->gop.virtAddr[(offsetY + y * GLYPH_HEIGHT) * screen->gop.stride],
                &newLine->pixels[offsetY * SCREEN_LINE_STRIDE], newLine->length * GLYPH_WIDTH * sizeof(uint32_t));
        }

        if (line->length > newLine->length)
        {
            for (uint64_t offsetY = 0; offsetY < GLYPH_HEIGHT; offsetY++)
            {
                memset32(&screen->gop.virtAddr[(offsetY + y * GLYPH_HEIGHT) * screen->gop.stride +
                             newLine->length * GLYPH_WIDTH],
                    0xFF000000, (line->length - newLine->length) * GLYPH_WIDTH);
            }
        }
    }

    screen_line_t* last = screen_buffer_get_line(&screen->buffer, newCursorY);
    for (uint64_t offsetY = 0; offsetY < GLYPH_HEIGHT; offsetY++)
    {
        memset32(&screen->gop.virtAddr[(offsetY + newCursorY * GLYPH_HEIGHT) * screen->gop.stride], 0xFF000000,
            last->length * GLYPH_WIDTH);
    }

    screen->cursor.y = newCursorY;
    screen->buffer.invalidStart = (screen_pos_t){0, 0};
    screen->buffer.invalidEnd = (screen_pos_t){0, 0};
    screen->buffer.firstLineIndex = (screen->buffer.firstLineIndex + 1) % screen->buffer.height;

    screen_line_t* currentLine = screen_buffer_get_line(&screen->buffer, screen->cursor.y);
    currentLine->length = 0;
}

static void screen_advance_cursor(screen_t* screen, char chr, bool shouldScroll)
{
    screen_line_t* line = &screen->buffer.lines[screen->cursor.y];

    if (chr == '\n')
    {
        screen->cursor.y++;
        screen->cursor.x = 0;

        if (screen->cursor.y >= screen->buffer.height && shouldScroll)
        {
            screen_scroll(screen);
        }
    }
    else if (screen->cursor.x >= screen->buffer.width - 1)
    {
        screen->cursor.y++;
        screen->cursor.x = 0;

        if (screen->cursor.y >= screen->buffer.height && shouldScroll)
        {
            screen_scroll(screen);
        }

        for (uint64_t i = 0; i < SCREEN_WRAP_INDENT; i++)
        {
            if (shouldScroll)
            {
                screen_buffer_put(&screen->buffer, &screen->cursor, ' ');
            }
            screen->cursor.x++;
        }
    }
    else
    {
        screen->cursor.x++;
    }
}

void screen_enable(screen_t* screen, const ring_t* ring)
{
    screen_clear(screen);
    screen->cursor.x = 0;
    screen->cursor.y = 0;
    if (ring == NULL)
    {
        return;
    }

    for (uint64_t i = 0; i < ring_data_length(ring); i++)
    {
        uint8_t chr;
        ring_get_byte(ring, i, &chr);

        screen_advance_cursor(screen, chr, false);
    }

    uint64_t totalLineAmount = screen->cursor.y;

    screen->cursor.x = 0;
    screen->cursor.y = 0;

    uint64_t i = 0;
    for (; i < ring_data_length(ring) && (totalLineAmount - screen->cursor.y) > screen->buffer.height - 1; i++)
    {
        uint8_t chr;
        ring_get_byte(ring, i, &chr);

        screen_advance_cursor(screen, chr, false);
    }

    screen->cursor.x = 0;
    screen->cursor.y = 0;

    screen_pos_t prevCursor = screen->cursor;
    for (; i < ring_data_length(ring); i++)
    {
        uint8_t chr;
        ring_get_byte(ring, i, &chr);

        screen_buffer_put(&screen->buffer, &screen->cursor, chr);
        screen_advance_cursor(screen, chr, false);
    }

    screen_buffer_flush(&screen->buffer, &screen->gop);
}

void screen_disable(screen_t* screen)
{
    screen_clear(screen);
}

void screen_write(screen_t* screen, const char* string, uint64_t length)
{
    screen_pos_t prevCursor = screen->cursor;

    for (uint64_t i = 0; i < length; i++)
    {
        char chr = string[i];
        screen_buffer_put(&screen->buffer, &screen->cursor, chr);
        screen_advance_cursor(screen, chr, true);
    }

    screen_buffer_flush(&screen->buffer, &screen->gop);
}

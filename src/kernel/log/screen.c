#include "screen.h"

#include "glyphs.h"
#include "mem/heap.h"
#include "sched/thread.h"

#include <assert.h>
#include <errno.h>
#include <sys/math.h>

static screen_line_t* screen_buffer_get_line(const screen_buffer_t* buffer, uint64_t y)
{
    uint64_t index = (y + buffer->firstLineIndex) % buffer->height;

    return buffer->lines[index];
}

static void screen_buffer_draw(screen_buffer_t* buffer, const screen_pos_t* cursor, char chr)
{
    if (chr == '\n' || chr < ' ')
    {
        chr = ' ';
    }

    const glyph_cache_t* cache = glyph_cache_get();
    const glyph_t* glyph = &cache->glyphs[(uint8_t)chr];

    screen_line_t* line = screen_buffer_get_line(buffer, cursor->y);

    uint64_t pixelX = cursor->x * GLYPH_WIDTH;

    for (uint64_t y = 0; y < GLYPH_HEIGHT; y++)
    {
        memcpy(&line->pixels[pixelX + y * buffer->stride], &glyph->pixels[y * GLYPH_WIDTH],
            sizeof(uint32_t) * GLYPH_WIDTH);
    }

    if (buffer->invalidStart.x == 0 && buffer->invalidStart.y == 0 && buffer->invalidEnd.x == 0 &&
        buffer->invalidEnd.y == 0)
    {
        buffer->invalidStart = *cursor;
        buffer->invalidEnd = (screen_pos_t){cursor->x + 1, cursor->y + 1};
    }
    else
    {
        buffer->invalidStart.x = MIN(buffer->invalidStart.x, cursor->x);
        buffer->invalidStart.y = MIN(buffer->invalidStart.y, cursor->y);
        buffer->invalidEnd.x = MAX(buffer->invalidEnd.x, cursor->x + 1);
        buffer->invalidEnd.y = MAX(buffer->invalidEnd.y, cursor->y + 1);
    }

    line->length = MAX(line->length, cursor->x + 1);
}

static void screen_buffer_flush(screen_buffer_t* buffer, gop_buffer_t* framebuffer)
{
    for (uint64_t y = buffer->invalidStart.y; y < buffer->invalidEnd.y; y++)
    {
        screen_line_t* line = screen_buffer_get_line(buffer, y);

        for (uint64_t pixelY = 0; pixelY < GLYPH_HEIGHT; pixelY++)
        {
            memcpy(&framebuffer
                       ->base[buffer->invalidStart.x * GLYPH_WIDTH + (pixelY + y * GLYPH_HEIGHT) * framebuffer->stride],
                &line->pixels[buffer->invalidStart.x * GLYPH_WIDTH + pixelY * buffer->stride],
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
    memset(buffer->storage, 0, buffer->lineSize * buffer->height);
}

static uint64_t screen_buffer_init(screen_buffer_t* buffer, uint64_t width, uint64_t height)
{
    buffer->width = width / GLYPH_WIDTH;
    buffer->height = height / GLYPH_HEIGHT;
    buffer->stride = buffer->width * GLYPH_WIDTH;
    buffer->lineSize = sizeof(screen_line_t) + sizeof(uint32_t) * buffer->stride * GLYPH_HEIGHT;
    buffer->firstLineIndex = 0;
    buffer->invalidStart = (screen_pos_t){0, 0};
    buffer->invalidEnd = (screen_pos_t){0, 0};

    buffer->lines = heap_calloc(buffer->height, sizeof(screen_line_t*), HEAP_VMM);
    buffer->storage = heap_calloc(1, buffer->lineSize * buffer->height, HEAP_VMM);
    if (buffer->lines == NULL || buffer->storage == NULL)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < buffer->height; i++)
    {
        buffer->lines[i] = (screen_line_t*)(buffer->storage + i * buffer->lineSize);
    }

    return 0;
}

static void screen_clear(screen_t* screen)
{
    screen_buffer_clear(&screen->buffer);

    for (uint64_t y = 0; y < screen->framebuffer.height; y++)
    {
        memset(&screen->framebuffer.base[y * screen->framebuffer.stride], 0,
            screen->framebuffer.width * sizeof(uint32_t));
    }
}

uint64_t screen_init(screen_t* screen, const gop_buffer_t* framebuffer)
{
    screen->initialized = true;
    screen->framebuffer = *framebuffer;
    screen->cursor = (screen_pos_t){0, 0};

    if (screen_buffer_init(&screen->buffer, framebuffer->width, framebuffer->height) == ERR)
    {
        return ERROR(ENOMEM);
    }

    return 0;
}

static void screen_scroll(screen_t* screen)
{
    screen->cursor.y = screen->cursor.y != 0 ? screen->cursor.y - 1 : 0;
    screen->buffer.invalidStart = (screen_pos_t){0, 0};
    screen->buffer.invalidEnd = (screen_pos_t){0, 0};
    screen->buffer.firstLineIndex = (screen->buffer.firstLineIndex + 1) % screen->buffer.height;

    screen_line_t* current = screen_buffer_get_line(&screen->buffer, screen->cursor.y);
    for (uint64_t pixelY = 0; pixelY < GLYPH_HEIGHT; pixelY++)
    {
        memset(&current->pixels[pixelY * screen->buffer.stride], 0, screen->buffer.stride * sizeof(uint32_t));
    }

    uint64_t prevLength = current->length;
    for (uint64_t y = 0; y < screen->buffer.height; y++)
    {
        screen_line_t* line = screen_buffer_get_line(&screen->buffer, y);

        uint64_t lengthToRedraw = MAX(prevLength, line->length);

        for (uint64_t pixelY = 0; pixelY < GLYPH_HEIGHT; pixelY++)
        {
            memcpy(&screen->framebuffer.base[(pixelY + y * GLYPH_HEIGHT) * screen->framebuffer.stride],
                &line->pixels[pixelY * screen->buffer.stride], lengthToRedraw * GLYPH_WIDTH * sizeof(uint32_t));
        }

        prevLength = line->length;
    }
    current->length = 0;
}

static void screen_advance_cursor(screen_t* screen, char chr, bool shouldScroll)
{
    screen_line_t* line = screen->buffer.lines[screen->cursor.y];

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
        screen->cursor.x = SCREEN_WRAP_INDENT;

        if (screen->cursor.y >= screen->buffer.height && shouldScroll)
        {
            screen_scroll(screen);
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

        screen_buffer_draw(&screen->buffer, &screen->cursor, chr);
        screen_advance_cursor(screen, chr, false);
    }

    screen_buffer_flush(&screen->buffer, &screen->framebuffer);
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
        screen_buffer_draw(&screen->buffer, &screen->cursor, chr);
        screen_advance_cursor(screen, chr, true);
    }

    screen_buffer_flush(&screen->buffer, &screen->framebuffer);
}
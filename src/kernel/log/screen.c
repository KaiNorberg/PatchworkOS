#include <kernel/log/screen.h>

#include <kernel/drivers/com.h>
#include <kernel/init/boot_info.h>
#include <kernel/log/glyphs.h>
#include <kernel/log/panic.h>
#include <kernel/sync/lock.h>

#include <string.h>
#include <sys/math.h>

static bool hidden = false;

static boot_gop_t gop = {0};
static screen_pos_t cursor = {0, 0};

static uint32_t width;
static uint32_t height;
static uint64_t offset;
static screen_pos_t invalidStart;
static screen_pos_t invalidEnd;
static screen_line_t lines[CONFIG_SCREEN_MAX_LINES];

static lock_t lock = LOCK_CREATE();

static screen_line_t* screen_get_line(size_t y)
{
    size_t index = (y + offset) % height;
    return &lines[index];
}

static void screen_invalidate(const screen_pos_t* pos)
{
    if (invalidStart.x == 0 && invalidStart.y == 0 && invalidEnd.x == 0 && invalidEnd.y == 0)
    {
        invalidStart = *pos;
        invalidEnd = (screen_pos_t){pos->x + 1, pos->y + 1};
    }
    else
    {
        invalidStart.x = MIN(invalidStart.x, pos->x);
        invalidStart.y = MIN(invalidStart.y, pos->y);
        invalidEnd.x = MAX(invalidEnd.x, pos->x + 1);
        invalidEnd.y = MAX(invalidEnd.y, pos->y + 1);
    }

    invalidEnd.x = MIN(invalidEnd.x, width);
    invalidEnd.y = MIN(invalidEnd.y, height);
}

static void screen_put(char chr)
{
    if (chr == '\n' || chr < ' ')
    {
        chr = ' ';
    }

    const glyph_cache_t* cache = glyph_cache_get();
    const glyph_t* glyph = &cache->glyphs[(uint8_t)chr];

    screen_line_t* line = screen_get_line(cursor.y);

    uint64_t pixelX = cursor.x * GLYPH_WIDTH;
    for (uint64_t y = 0; y < GLYPH_HEIGHT; y++)
    {
        memcpy(&line->pixels[pixelX + y * SCREEN_LINE_STRIDE], &glyph->pixels[y * GLYPH_WIDTH],
            sizeof(uint32_t) * GLYPH_WIDTH);
    }

    line->length = MAX(line->length, cursor.x + 1);
    screen_invalidate(&cursor);
}

static void screen_flush(void)
{
    if (hidden)
    {
        goto flush;
    }

    for (uint64_t y = invalidStart.y; y < invalidEnd.y; y++)
    {
        screen_line_t* line = screen_get_line(y);

        for (uint64_t pixelY = 0; pixelY < GLYPH_HEIGHT; pixelY++)
        {
            memcpy(&gop.virtAddr[invalidStart.x * GLYPH_WIDTH + (y * GLYPH_HEIGHT + pixelY) * gop.stride],
                &line->pixels[invalidStart.x * GLYPH_WIDTH + pixelY * SCREEN_LINE_STRIDE],
                (MIN(invalidEnd.x, line->length) - invalidStart.x) * GLYPH_WIDTH * sizeof(uint32_t));
        }
    }

flush:
    invalidStart = (screen_pos_t){0, 0};
    invalidEnd = (screen_pos_t){0, 0};
}

void screen_init(void)
{
    boot_info_t* bootInfo = boot_info_get();
    if (bootInfo == NULL)
    {
        panic(NULL, "screen_init: boot info is NULL");
    }

    gop = bootInfo->gop;

    width = MIN(gop.width / GLYPH_WIDTH, SCREEN_LINE_MAX_LENGTH);
    height = MIN(gop.height / GLYPH_HEIGHT, CONFIG_SCREEN_MAX_LINES);

    memset(gop.virtAddr, 0, gop.height * gop.stride * sizeof(uint32_t));
}

static void screen_scroll(void)
{
    uint64_t newCursorY = cursor.y != 0 ? cursor.y - 1 : 0;

    if (hidden)
    {
        goto flush;
    }

    for (uint64_t y = 0; y < newCursorY; y++)
    {
        screen_line_t* line = screen_get_line(y);
        screen_line_t* newLine = screen_get_line(y + 1);

        for (uint64_t offsetY = 0; offsetY < GLYPH_HEIGHT; offsetY++)
        {
            memcpy(&gop.virtAddr[(offsetY + y * GLYPH_HEIGHT) * gop.stride],
                &newLine->pixels[offsetY * SCREEN_LINE_STRIDE], newLine->length * GLYPH_WIDTH * sizeof(uint32_t));
        }

        if (line->length > newLine->length)
        {
            for (uint64_t offsetY = 0; offsetY < GLYPH_HEIGHT; offsetY++)
            {
                memset32(&gop.virtAddr[(offsetY + y * GLYPH_HEIGHT) * gop.stride + newLine->length * GLYPH_WIDTH],
                    0xFF000000, (line->length - newLine->length) * GLYPH_WIDTH);
            }
        }
    }

    screen_line_t* last = screen_get_line(newCursorY);
    for (uint64_t offsetY = 0; offsetY < GLYPH_HEIGHT; offsetY++)
    {
        memset32(&gop.virtAddr[(offsetY + newCursorY * GLYPH_HEIGHT) * gop.stride], 0xFF000000,
            last->length * GLYPH_WIDTH);
    }

flush:
    cursor.y = newCursorY;
    invalidStart = (screen_pos_t){0, 0};
    invalidEnd = (screen_pos_t){0, 0};
    offset = (offset + 1) % height;

    screen_line_t* currentLine = screen_get_line(cursor.y);
    currentLine->length = 0;
}

static void screen_advance_cursor(char chr)
{
    if (chr == '\n')
    {
        cursor.y++;
        cursor.x = 0;

        if (cursor.y >= height)
        {
            screen_scroll();
        }
    }
    else if (cursor.x >= width)
    {
        cursor.y++;
        cursor.x = 0;

        if (cursor.y >= height)
        {
            screen_scroll();
        }

        for (uint64_t i = 0; i < SCREEN_WRAP_INDENT; i++)
        {
            screen_put(' ');
            cursor.x++;
        }
    }
    else
    {
        cursor.x++;
    }
}

void screen_hide(void)
{
    LOCK_SCOPE(&lock);
    hidden = true;
}

void screen_show(void)
{
    LOCK_SCOPE(&lock);
    hidden = false;

    memset(gop.virtAddr, 0, gop.height * gop.stride * sizeof(uint32_t));

    invalidStart = (screen_pos_t){0, 0};
    invalidEnd = (screen_pos_t){width, height};
    screen_flush();
}

uint64_t screen_get_width(void)
{
    return width;
}

uint64_t screen_get_height(void)
{
    return height;
}

void screen_write(const char* string, uint64_t length)
{
    LOCK_SCOPE(&lock);

    for (uint64_t i = 0; i < length; i++)
    {
        char chr = string[i];
        screen_put(chr);
        screen_advance_cursor(chr);
    }

    screen_flush();
}

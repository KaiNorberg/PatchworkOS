#include <kernel/log/log_screen.h>

#include <kernel/drivers/com.h>
#include <kernel/log/glyphs.h>
#include <kernel/sync/lock.h>

#include <string.h>
#include <sys/math.h>

static boot_gop_t gop = {0};
static log_screen_pos_t cursor = {0, 0};
static log_screen_t screen = {0};

static lock_t lock = LOCK_CREATE();

static log_screen_line_t* log_screen_get_line(uint64_t y)
{
    uint64_t index = (y + screen.firstLineIndex) % screen.height;
    return &screen.lines[index];
}

static void log_screen_invalidate(const log_screen_pos_t* pos)
{
    if (screen.invalidStart.x == 0 && screen.invalidStart.y == 0 && screen.invalidEnd.x == 0 &&
        screen.invalidEnd.y == 0)
    {
        screen.invalidStart = *pos;
        screen.invalidEnd = (log_screen_pos_t){pos->x + 1, pos->y + 1};
    }
    else
    {
        screen.invalidStart.x = MIN(screen.invalidStart.x, pos->x);
        screen.invalidStart.y = MIN(screen.invalidStart.y, pos->y);
        screen.invalidEnd.x = MAX(screen.invalidEnd.x, pos->x + 1);
        screen.invalidEnd.y = MAX(screen.invalidEnd.y, pos->y + 1);
    }

    screen.invalidEnd.x = MIN(screen.invalidEnd.x, screen.width);
    screen.invalidEnd.y = MIN(screen.invalidEnd.y, screen.height);
}

static void log_screen_put(char chr)
{
    if (chr == '\n' || chr < ' ')
    {
        chr = ' ';
    }

    const glyph_cache_t* cache = glyph_cache_get();
    const glyph_t* glyph = &cache->glyphs[(uint8_t)chr];

    log_screen_line_t* line = log_screen_get_line(cursor.y);

    uint64_t pixelX = cursor.x * GLYPH_WIDTH;
    for (uint64_t y = 0; y < GLYPH_HEIGHT; y++)
    {
        memcpy(&line->pixels[pixelX + y * SCREEN_LINE_STRIDE], &glyph->pixels[y * GLYPH_WIDTH],
            sizeof(uint32_t) * GLYPH_WIDTH);
    }

    line->length = MAX(line->length, cursor.x + 1);
    log_screen_invalidate(&cursor);
}

static void log_screen_flush(void)
{
    for (uint64_t y = screen.invalidStart.y; y < screen.invalidEnd.y; y++)
    {
        log_screen_line_t* line = log_screen_get_line(y);

        for (uint64_t pixelY = 0; pixelY < GLYPH_HEIGHT; pixelY++)
        {
            memcpy(&gop.virtAddr[screen.invalidStart.x * GLYPH_WIDTH + (y * GLYPH_HEIGHT + pixelY) * gop.stride],
                &line->pixels[screen.invalidStart.x * GLYPH_WIDTH + pixelY * SCREEN_LINE_STRIDE],
                (screen.invalidEnd.x - screen.invalidStart.x) * GLYPH_WIDTH * sizeof(uint32_t));
        }
    }

    screen.invalidStart = (log_screen_pos_t){0, 0};
    screen.invalidEnd = (log_screen_pos_t){0, 0};
}

void log_screen_init(const boot_gop_t* bootGop)
{
    gop = *bootGop;
    cursor = (log_screen_pos_t){0, 0};

    screen.width = MIN(gop.width / GLYPH_WIDTH, SCREEN_LINE_MAX_LENGTH);
    screen.height = MIN(gop.height / GLYPH_HEIGHT, CONFIG_SCREEN_MAX_LINES);
    screen.firstLineIndex = 0;
    screen.invalidStart = (log_screen_pos_t){0, 0};
    screen.invalidEnd = (log_screen_pos_t){0, 0};
    memset(screen.lines, 0, sizeof(screen.lines));

    log_screen_clear();
}

static void log_screen_scroll(void)
{
    uint64_t newCursorY = cursor.y != 0 ? cursor.y - 1 : 0;
    for (uint64_t y = 0; y < newCursorY; y++)
    {
        log_screen_line_t* line = log_screen_get_line(y);
        log_screen_line_t* newLine = log_screen_get_line(y + 1);

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

    log_screen_line_t* last = log_screen_get_line(newCursorY);
    for (uint64_t offsetY = 0; offsetY < GLYPH_HEIGHT; offsetY++)
    {
        memset32(&gop.virtAddr[(offsetY + newCursorY * GLYPH_HEIGHT) * gop.stride], 0xFF000000,
            last->length * GLYPH_WIDTH);
    }

    cursor.y = newCursorY;
    screen.invalidStart = (log_screen_pos_t){0, 0};
    screen.invalidEnd = (log_screen_pos_t){0, 0};
    screen.firstLineIndex = (screen.firstLineIndex + 1) % screen.height;

    log_screen_line_t* currentLine = log_screen_get_line(cursor.y);
    currentLine->length = 0;
}

static void log_screen_advance_cursor(char chr)
{
    if (chr == '\n')
    {
        cursor.y++;
        cursor.x = 0;

        if (cursor.y >= screen.height)
        {
            log_screen_scroll();
        }
    }
    else if (cursor.x >= screen.width)
    {
        cursor.y++;
        cursor.x = 0;

        if (cursor.y >= screen.height)
        {
            log_screen_scroll();
        }

        for (uint64_t i = 0; i < SCREEN_WRAP_INDENT; i++)
        {
            log_screen_put(' ');
            cursor.x++;
        }
    }
    else
    {
        cursor.x++;
    }
}

void log_screen_clear(void)
{
    LOCK_SCOPE(&lock);

    cursor = (log_screen_pos_t){0, 0};

    screen.firstLineIndex = 0;
    screen.invalidStart = (log_screen_pos_t){0, 0};
    screen.invalidEnd = (log_screen_pos_t){0, 0};

    for (uint64_t i = 0; i < screen.height; i++)
    {
        screen.lines[i].length = 0;
        memset(screen.lines[i].pixels, 0, sizeof(screen.lines[i].pixels));
    }

    for (uint64_t y = 0; y < gop.height; y++)
    {
        memset32(&gop.virtAddr[y * gop.stride], 0xFF000000, gop.width);
    }
}

uint64_t log_screen_get_width(void)
{
    return screen.width;
}

uint64_t log_screen_get_height(void)
{
    return screen.height;
}

void log_screen_write(const char* string, uint64_t length)
{
    LOCK_SCOPE(&lock);

    for (uint64_t i = 0; i < length; i++)
    {
        char chr = string[i];
        log_screen_put(chr);
        log_screen_advance_cursor(chr);
    }

    log_screen_flush();
}

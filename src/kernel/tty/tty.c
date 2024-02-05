#include "tty.h"

#include "utils/utils.h"
#include "lock/lock.h"

#include <libc/string.h>

static Framebuffer* frontbuffer;
static PsfFont* font;

static Point cursorPos;

static Pixel background;
static Pixel foreground;

static uint8_t scale;

static Lock lock;

void tty_init(Framebuffer* screenbuffer, PsfFont* screenFont)
{
    frontbuffer = screenbuffer;
    font = screenFont;

    cursorPos.x = 0;
    cursorPos.y = 0;

    background.a = 0;
    background.r = 0;
    background.g = 0;
    background.b = 0;
    
    foreground.a = 255;
    foreground.r = 255;
    foreground.g = 255;
    foreground.b = 255;

    scale = 1;
        
    lock = lock_new();

    tty_clear();
}

void tty_acquire()
{
    lock_acquire(&lock);
}

void tty_release()
{
    lock_release(&lock);
}

void tty_scroll(uint64_t distance)
{
    cursorPos.y -= distance;

    uint64_t offset = frontbuffer->pixelsPerScanline * distance;
    memcpy(frontbuffer->base, frontbuffer->base + offset, frontbuffer->size - offset * sizeof(Pixel));
    memset(frontbuffer->base + frontbuffer->pixelsPerScanline * (frontbuffer->height - distance), 0, offset * sizeof(Pixel));
}

void tty_put(uint8_t chr)
{
    switch (chr)
    {
    case '\n':
    {
        cursorPos.x = 0;
        cursorPos.y += TTY_CHAR_HEIGHT * scale;
        if (cursorPos.y + TTY_CHAR_HEIGHT * scale >= frontbuffer->height)
        {
            tty_scroll(TTY_CHAR_HEIGHT * scale);
        }
    }
    break;
    case '\r':
    {
        cursorPos.x = 0;
    }
    break;
    default:
    {               
        char const* glyph = font->glyphs + chr * TTY_CHAR_HEIGHT;

        for (uint32_t y = 0; y < TTY_CHAR_HEIGHT * scale; y++)
        {
            for (uint32_t x = 0; x < TTY_CHAR_WIDTH * scale; x++)
            {
                Point position = {cursorPos.x + x, cursorPos.y + y};

                if ((*glyph & (0b10000000 >> x / scale)) > 0)
                {
                    GOP_PUT(frontbuffer, position, foreground);
                }
                else
                {
                    GOP_PUT(frontbuffer, position, background);
                }
            }
            if (y % scale == 0)
            {
                glyph++;
            }
        }

        cursorPos.x += TTY_CHAR_WIDTH * scale;

        if (cursorPos.x >= frontbuffer->width)
        {
            cursorPos.x = 0;
            cursorPos.y += TTY_CHAR_HEIGHT * scale;
        }
    }
    break;
    }
}

void tty_print(const char* string)
{
    char* chr = (char*)string;
    while (*chr != '\0')
    {
        tty_put(*chr);
        chr++;
    }
}

void tty_printi(uint64_t integer)
{
    char string[64];
    itoa(integer, string, 10);
    tty_print(string);
}

void tty_printx(uint64_t hex)
{
    char string[64];
    memset(string, 0, 64);
    itoa(hex, string, 16);
    tty_print("0x"); tty_print(string);
}

void tty_clear()
{
    memset(frontbuffer->base, 0, frontbuffer->size);
    cursorPos.x = 0;
    cursorPos.y = 0;
}

void tty_set_scale(uint8_t newScale)
{
    scale = newScale;
}

void tty_set_foreground(Pixel color)
{
    foreground = color;
}

void tty_set_background(Pixel color)
{
    background = color;
}

void tty_set_cursor_pos(uint64_t x, uint64_t y)
{
    cursorPos.x = x;
    cursorPos.y = y;
}

Point tty_get_cursor_pos()
{
    return cursorPos;
}

uint32_t tty_get_screen_width()
{
    return frontbuffer->width;
}

uint32_t tty_get_screen_height()
{
    return frontbuffer->height;
}

void tty_start_message(const char* message)
{
    tty_print("[..] ");
    tty_print(message);
    tty_print("... ");
}

void tty_assert(uint8_t expression, const char* message)
{
    if (!expression)
    {
        tty_print(message);
        tty_end_message(TTY_MESSAGE_ER);
    }
}

void tty_end_message(uint64_t status)
{
    uint32_t oldCursorX = cursorPos.x;   
    cursorPos.x = TTY_CHAR_WIDTH * scale;

    if (status == TTY_MESSAGE_OK)
    {
        foreground.a = 255;
        foreground.r = 0;
        foreground.g = 255;
        foreground.b = 0;
        tty_print("OK");
    }
    else if (status == TTY_MESSAGE_ER)
    {
        foreground.a = 255;
        foreground.r = 255;
        foreground.g = 0;
        foreground.b = 0;
        tty_print("ER");

        while (1)
        {
            asm volatile ("HLT");
        }
    }
    else
    {
        //Undefined behaviour
    }

    foreground.a = 255;
    foreground.r = 255;
    foreground.g = 255;
    foreground.b = 255;
    
    foreground.a = 255;
    foreground.r = 255;
    foreground.g = 255;
    foreground.b = 255;
    cursorPos.x = oldCursorX;

    tty_print("done!\n\r");
}
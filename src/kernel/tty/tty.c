#include "tty.h"

#include "string/string.h"
#include "utils/utils.h"

Framebuffer* frontbuffer;
PSFFont* font;

Point cursorPos;

Pixel background;
Pixel foreground;

uint8_t textScale;

void tty_init(Framebuffer* screenbuffer, PSFFont* screenFont)
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

    textScale = 1;

    tty_clear();
}

void tty_put(uint8_t chr)
{
    char* glyph = font->glyphs + chr * 16;

    switch (chr)
    {
    case '\n':
    {
        cursorPos.x = 0;
        cursorPos.y += 16 * textScale;
    }
    break;
    case '\r':
    {
        cursorPos.x = 0;
    }
    break;
    default:
    {
        for (uint64_t y = 0; y < 16 * textScale; y++)
        {
            for (uint64_t x = 0; x < 8 * textScale; x++)
            {
                Pixel pixel;

                if ((*glyph & (0b10000000 >> x / textScale)) > 0)
                {
                    pixel = foreground;
                }
                else
                {
                    pixel = background;
                }

                Point position = {cursorPos.x + x, cursorPos.y + y};

                gop_put(frontbuffer, position, pixel);
            }
            if (y % textScale == 0)
            {
                glyph++;
            }
        }

        cursorPos.x += 8 * textScale;

        if (cursorPos.x >= frontbuffer->width)
        {
            cursorPos.x = 0;
            cursorPos.y += 16 * textScale;
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
    itoa(hex, string, 16);
    tty_print("0x"); tty_print(string);
}

void tty_clear()
{
    memset(frontbuffer->base, 0, frontbuffer->size);
    cursorPos.x = 0;
    cursorPos.y = 0;
}

void tty_set_scale(uint8_t scale)
{
    textScale = scale;
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

void tty_start_message(const char* message)
{
    tty_print("[..] ");
    tty_print(message);
    tty_print("...");
}

void tty_end_message(uint64_t status)
{
    uint64_t oldCursorX = cursorPos.x;   
    cursorPos.x = 8 * textScale;

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

    tty_print(" done!\n\r");
}
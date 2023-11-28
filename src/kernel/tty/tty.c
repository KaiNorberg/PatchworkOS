#include "tty.h"

#include "libc/include/string.h"
#include "kernel/utils/utils.h"

Framebuffer* frontbuffer;
PSFFont* font;

Point cursorPos;

Pixel background;
Pixel foreground;

uint8_t scale;

void tty_init(Framebuffer* screenbuffer, PSFFont* screenFont)
{
    frontbuffer = screenbuffer;
    font = screenFont;

    cursorPos.X = 0;
    cursorPos.Y = 0;

    background.A = 0;
    background.R = 0;
    background.G = 0;
    background.B = 0;
    
    foreground.A = 255;
    foreground.R = 255;
    foreground.G = 255;
    foreground.B = 255;

    scale = 1;

    tty_clear();
}

void tty_put(uint8_t chr)
{
    char* glyph = font->Glyphs + (chr * font->Header->charSize);

    if (chr == '\n')
    {
        cursorPos.X = 0;
        cursorPos.Y += 16 * scale;
    }
    else if (chr == '\r')
    {
        cursorPos.X = 0;
    }
    else
    {
        for (uint64_t y = 0; y < 16 * scale; y++)
        {
            for (uint64_t x = 0; x < 8 * scale; x++)
            {
                Pixel pixel;

                if ((*glyph & (0b10000000 >> x / scale)) > 0)
                {
                    pixel = foreground;
                }
                else
                {
                    pixel = background;
                }

                Point position = {cursorPos.X + x, cursorPos.Y + y};

                gop_put(frontbuffer, position, pixel);
            }
            if (y % scale == 0)
            {
                glyph++;
            }
        }

        cursorPos.X += 8;

        if (cursorPos.X >= frontbuffer->Width)
        {
            cursorPos.X = 0;
            cursorPos.Y += 16 * scale;
        }
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
    itoa(integer, string);
    tty_print(string);
}

void tty_clear()
{
    memset(frontbuffer->Base, 0, frontbuffer->Size);
    cursorPos.X = 0;
    cursorPos.Y = 0;
}

void tty_set_foreground(Pixel color)
{
    foreground = color;
}

void tty_set_background(Pixel color)
{
    background = color;
}

void tty_start_message(const char* message)
{
    tty_print("[..] ");
    tty_print(message);
    tty_print("...");
}

void tty_end_message(uint64_t status)
{
    uint64_t oldCursorX = cursorPos.X;   
    cursorPos.X = 8;

    if (status == TTY_MESSAGE_OK)
    {
        foreground.A = 255;
        foreground.R = 0;
        foreground.G = 255;
        foreground.B = 0;
        tty_print("OK");
    }
    else if (status == TTY_MESSAGE_ER)
    {
        foreground.A = 255;
        foreground.R = 255;
        foreground.G = 0;
        foreground.B = 0;
        tty_print("ER");
    }
    else
    {
        //Undefined behaviour
    }

    foreground.A = 255;
    foreground.R = 255;
    foreground.G = 255;
    foreground.B = 255;
    
    if (status != TTY_MESSAGE_ER)
    {
        foreground.A = 255;
        foreground.R = 255;
        foreground.G = 255;
        foreground.B = 255;
        cursorPos.X = oldCursorX;
        tty_print(" done!");
    }

    tty_print("\n\r");
}
#include "tty.h"

#include "libc/include/string.h"
#include "kernel/utils/utils.h"

Framebuffer* frontbuffer;
PSFFont* font;

Point cursorPos;

Pixel background;
Pixel foreground;

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

    memset(frontbuffer->Base, 0, frontbuffer->Size);
}

void tty_put(uint8_t chr)
{
    char* glyph = font->Glyphs + (chr * font->Header->charSize);

    if (chr == '\n')
    {
        cursorPos.X = 0;
        cursorPos.Y += 16;
    }
    else
    {
        for (uint64_t y = 0; y < 16; y++)
        {
            for (uint64_t x = 0; x < 8; x++)
            {
                Pixel pixel;

                if ((*glyph & (0b10000000 >> x)) > 0)
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
            glyph++;
        }

        cursorPos.X += 8;

        if (cursorPos.X >= frontbuffer->Width)
        {
            cursorPos.X = 0;
            cursorPos.Y += 16;
        }
    }
}

void tty_print(const char* string)
{
    char* chr = string;
    while (*chr != '\0')
    {
        tty_put(*chr);
        chr++;
    }

    cursorPos.X = 0;
    cursorPos.Y += 16;
}

void tty_printi(uint64_t integer)
{
    char string[64];
    itoa(integer, string);
    tty_print(string);
}
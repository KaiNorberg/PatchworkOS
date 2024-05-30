#include "terminal.h"

#include "fb.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/proc.h>
#include <sys/kbd.h>

static Cursor cursor;
static TerminalState state;

static uint32_t foreground;
static uint32_t background;
static uint64_t scale;

static uint32_t colors[] = 
{
    0xFF1E2229,
    0xFFED1515,
    0xFF44853A,
    0xFFF67400,
    0xFF1984D1,
    0xFF9B59B6,
    0xFF1ABC9C,
    0xFFFCFCFC
};

static void terminal_char(char chr)
{
    fb_char(chr, cursor.x * FB_CHAR_WIDTH * scale, cursor.y * FB_CHAR_HEIGHT * scale, scale, foreground, background);
}

static uint64_t terminal_screen_height(void)
{
    return fb_height() / (FB_CHAR_HEIGHT * scale);
}

/*static uint64_t terminal_screen_width(void)
{
    return fb_width() / (FB_CHAR_WIDTH * scale);
}*/

static void terminal_put_normal(char chr)
{
    switch (chr)
    {
    case '\n':
    {
        terminal_char(' ');
        cursor.x = 0;
        cursor.y += 1;

        if (cursor.y >= terminal_screen_height())
        {
            cursor.y -= 1;
            fb_scroll(FB_CHAR_HEIGHT * scale);
        }
    }
    break;
    case '\b':
    {
        terminal_char(' ');
        if (cursor.x > 0)
        {
            cursor.x--;
        }
    }
    break;
    case '\t':
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            terminal_put(' ');
        }
    }
    break;
    default:
    {
        terminal_char(chr);
        cursor.x++;
    }
    break;
    }
}

void terminal_init(void)
{
    cursor.x = 0;
    cursor.y = 0;
    cursor.visible = true;
    cursor.nextBlink = uptime() + TERMINAL_BLINK_INTERVAL;
    state = TERMINAL_STATE_NORMAL;

    foreground = TERMINAL_FOREGROUND;
    background = TERMINAL_BACKGROUND;
    scale = 1;

    fb_clear(TERMINAL_BACKGROUND);

    terminal_print("Welcome to Patchwork OS!\n");
    terminal_print("This currently does absolutely nothing!\n");
}

void terminal_update_cursor(void)
{
    uint64_t time = uptime();
    if (cursor.nextBlink < time)
    {
        cursor.visible = !cursor.visible;
        cursor.nextBlink = time + TERMINAL_BLINK_INTERVAL;
    }

    if (cursor.visible)
    {
        fb_char(' ', cursor.x * FB_CHAR_WIDTH * scale, cursor.y * FB_CHAR_HEIGHT * scale, scale, background, foreground);
    }
    else
    {        
        fb_char(' ', cursor.x * FB_CHAR_WIDTH * scale, cursor.y * FB_CHAR_HEIGHT * scale, scale, foreground, background);     
    }
}

void terminal_clear(void)
{
    cursor.x = 0;
    cursor.y = 0;
    fb_clear(background);
}

void terminal_put(const char chr)
{
    //This is kinda bad but it works for now.

    switch (state)
    {
    case TERMINAL_STATE_NORMAL:
    {
        if (chr == '\033')
        {
            state = TERMINAL_STATE_ESCAPE_1;
            break;
        }

        terminal_put_normal(chr);
    }
    break;
    case TERMINAL_STATE_ESCAPE_1:
    {
        if (chr == '[')
        {
            state = TERMINAL_STATE_ESCAPE_2;
        }
        else
        {
            state = TERMINAL_STATE_NORMAL;
        }
    }
    break;
    case TERMINAL_STATE_ESCAPE_2:
    {
        if (chr == '3')
        {
            state = TERMINAL_STATE_FOREGROUND;
        }
        else if (chr == '4')
        {
            state = TERMINAL_STATE_BACKGROUND;
        }
        else if (chr == 'm')
        {
            foreground = TERMINAL_FOREGROUND;
            background = TERMINAL_BACKGROUND;
        }
        else
        {
            state = TERMINAL_STATE_NORMAL;
        }
    }
    break;
    case TERMINAL_STATE_FOREGROUND:
    {
        uint8_t selector = chr - '0' - 1;
        if (selector < TERMINAL_MAX_COLOR)
        {
            foreground = colors[selector];
        }
    }
    break;
    case TERMINAL_STATE_BACKGROUND:
    {
        uint8_t selector = chr - '0' - 1;
        if (selector <= TERMINAL_MAX_COLOR)
        {
            background = colors[selector];
        }
    }
    break;
    }

    if (state != TERMINAL_STATE_NORMAL && chr == 'm')
    {
        state = TERMINAL_STATE_NORMAL;
    }

    cursor.visible = true;
    cursor.nextBlink = uptime() + TERMINAL_BLINK_INTERVAL;
}

void terminal_print(const char* string)
{    
    for (uint64_t i = 0; i < strlen(string); i++)
    {
        terminal_put(string[i]);
    }
}

void terminal_error(const char* string)
{    
    terminal_print("error: ");
    terminal_print(string);
    
    //if (errno != 0)
    {
        terminal_print(" - (");
        terminal_print(strerror(errno));
        terminal_print(")");
    }

    terminal_print("\n");
}
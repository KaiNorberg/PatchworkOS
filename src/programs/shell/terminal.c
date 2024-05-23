#include "terminal.h"

#include "fb.h"
#include "ascii.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <sys/kbd.h>

static Cursor cursor;
static TerminalState state;

static bool capsLock;
static bool shift;

static fd_t keyboard;

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

static void terminal_cursor(void)
{
    if (cursor.visible)
    {
        fb_char(' ', cursor.x * FB_CHAR_WIDTH * scale, cursor.y * FB_CHAR_HEIGHT * scale, scale, background, foreground);    
    }
    else
    {
        fb_char(' ', cursor.x * FB_CHAR_WIDTH * scale, cursor.y * FB_CHAR_HEIGHT * scale, scale, foreground, background);     
    }
}

static uint64_t terminal_screen_height(void)
{
    return fb_height() / (FB_CHAR_HEIGHT * scale);
}

/*static uint64_t terminal_screen_width(void)
{
    return fb_width() / (FB_CHAR_WIDTH * scale);
}*/

static uint8_t terminal_read_key(void)
{
    while (1)
    {
        terminal_cursor();

        pollfd_t fds[] = 
        {
            {.fd = keyboard, .requested = POLL_READ}
        };
        poll(fds, 1, TERMINAL_BLINK_INTERVAL);

        kbd_event_t event;
        if (read(keyboard, &event, sizeof(kbd_event_t)) == sizeof(kbd_event_t))
        {
            if (event.code == KEY_CAPS_LOCK && event.type == KBD_EVENT_TYPE_PRESS)
            {
                capsLock = !capsLock;
            }
            if (event.code == KEY_LEFT_SHIFT)
            {
                shift = event.type == KBD_EVENT_TYPE_PRESS;
            }

            if (event.type == KBD_EVENT_TYPE_PRESS)
            {
                cursor.visible = true;
                return event.code;
            }
        }
        else
        {
            cursor.visible = !cursor.visible;
        }
    }
}

static char terminal_key_to_ascii(uint8_t key)
{
    if (capsLock || shift)
    {
        return shiftedKeyToAscii[key];
    }
    else
    {
        return keyToAscii[key];
    }
}

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
    state = TERMINAL_STATE_NORMAL;

    capsLock = 0;
    shift = 0;

    foreground = TERMINAL_FOREGROUND;
    background = TERMINAL_BACKGROUND;
    scale = 1;

    fb_clear(TERMINAL_BACKGROUND);

    keyboard = open("sys:/kbd/ps2");
    if (keyboard == ERR)
    {
        terminal_panic("Failed to open keyboard");
    }

    terminal_print("Welcome to Patchwork OS!\n");
    terminal_print("This currently does absolutely nothing!\n");
}

const char* terminal_read(void)
{
    static char command[TERMINAL_MAX_COMMAND];
    command[0] = '\0';

    char cwd[256];
    realpath(cwd, ".");

    terminal_print("\n");
    terminal_print(cwd);
    terminal_print("\n\033[35m>\033[m ");

    uint64_t length = 0;
    while (1)
    {
        uint8_t key = terminal_read_key();
        char ascii = terminal_key_to_ascii(key);

        if (key == KEY_BACKSPACE)
        {
            if (length != 0)
            {
                command[length - 1] = '0';

                terminal_put('\b');
                length--;
            }
        }
        else if (key == KEY_ENTER)
        {
            terminal_put('\n');
            return command;
        }
        else if (ascii != '\0' && length < TERMINAL_MAX_COMMAND)
        {
            command[length] = ascii;
            command[length + 1] = '\0';

            terminal_put(ascii);
            length++;
        }
    }
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
}

void terminal_print(const char* string)
{    
    for (uint64_t i = 0; i < strlen(string); i++)
    {
        terminal_put(string[i]);
    }
}

void terminal_panic(const char* string)
{
    terminal_print("\033[32mTERMINAL PANIC: ");
    terminal_print(string);

    if (errno != 0)
    {
        terminal_print(" (");
        terminal_print(strerror(errno));
        terminal_print(")");
    }

    exit(EXIT_FAILURE);
}
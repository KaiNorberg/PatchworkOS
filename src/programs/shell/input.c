#include "input.h"

#include "ascii.h"
#include "terminal.h"

#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>

static fd_t keyboard;

static bool capsLock;
static bool shift;

static char input_key_to_ascii(uint8_t key)
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

void input_init(void)
{
    keyboard = open("sys:/kbd/ps2");
    if (keyboard == ERR)
    {
        terminal_error("failed to open keyboard");
        exit(EXIT_FAILURE);
    }
}

char input_kbd_read(uint64_t timeout)
{
    if (poll1(keyboard, POLL_READ, timeout) == 0)
    {
        return '\0';
    }

    kbd_event_t event;
    if (read(keyboard, &event, sizeof(kbd_event_t)) != sizeof(kbd_event_t))
    {
        return '\0';
    }

    if (event.code == KEY_CAPS_LOCK && event.type == KBD_EVENT_TYPE_PRESS)
    {
        capsLock = !capsLock;
    }
    if (event.code == KEY_LEFT_SHIFT)
    {
        shift = event.type == KBD_EVENT_TYPE_PRESS;
    }

    if (event.type != KBD_EVENT_TYPE_PRESS)
    {
        return '\0';
    }

    return input_key_to_ascii(event.code);
}
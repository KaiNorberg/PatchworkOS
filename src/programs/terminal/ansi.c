#include "ansi.h"

#include "terminal.h"

#include <string.h>
#include <errno.h>
#include <ctype.h>

void ansi_kbd_to_receiving(ansi_receiving_t* ansi, const event_kbd_t* kbd)
{
    switch (kbd->code)
    {
    case KBD_BACKSPACE:
        ansi->buffer[0] = '\b';
        ansi->length = 1;
        break;
    case KBD_ENTER:
        ansi->buffer[0] = '\n';
        ansi->length = 1;
        break;
    case KBD_TAB:
        ansi->buffer[0] = '\t';
        ansi->length = 1;
        break;
    case KBD_DELETE:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = '3';
        ansi->buffer[3] = '~';
        ansi->length = 4;
        break;
    case KBD_UP:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = 'A';
        ansi->length = 3;
        break;
    case KBD_DOWN:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = 'B';
        ansi->length = 3;
        break;
    case KBD_RIGHT:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = 'C';
        ansi->length = 3;
        break;
    case KBD_LEFT:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = 'D';
        ansi->length = 3;
        break;
    case KBD_PAGE_UP:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = '5';
        ansi->buffer[3] = '~';
        ansi->length = 4;
        break;
    case KBD_PAGE_DOWN:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = '6';
        ansi->buffer[3] = '~';
        ansi->length = 4;
        ;
        break;
    case KBD_HOME:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = '7';
        ansi->buffer[3] = '~';
        ansi->length = 4;
        break;
    case KBD_END:
        ansi->buffer[0] = '\033';
        ansi->buffer[1] = '[';
        ansi->buffer[2] = '8';
        ansi->buffer[3] = '~';
        ansi->length = 4;
        break;
    default:
        if (kbd->mods & KBD_MOD_CTRL && kbd->code == KBD_C)
        {
            ansi->buffer[0] = '\003';
            ansi->length = 1;
            return;
        }
        if (kbd->ascii >= 32 && kbd->ascii < 126)
        {
            ansi->buffer[0] = kbd->ascii;
            ansi->length = 1;
            return;
        }
        ansi->length = 0;
        break;
    }
}

void ansi_sending_init(ansi_sending_t* ansi)
{
    memset(ansi->buffer, 0, sizeof(ansi->buffer));
    ansi->length = 0;
    memset(ansi->parameters, 0, sizeof(ansi->parameters));
    ansi->paramCount = 0;
    ansi->command = '\0';
    ansi->ascii = false;
}

bool ansi_sending_parse(ansi_sending_t* ansi, char chr)
{
    if (ansi->length >= ANSI_MAX_LENGTH - 1)
    {
        ansi_sending_init(ansi);
        return false;
    }

    ansi->buffer[ansi->length++] = chr;
    ansi->buffer[ansi->length] = '\0';

    if (ansi->length == 1)
    {
        switch (ansi->buffer[0])
        {
        case '\033':
            ansi->parameters[0] = 0; // Reset
            ansi->paramCount = 0;
            return false;
        default:
            ansi->command = ansi->buffer[0];
            ansi->ascii = true;
            ansi->length = 0;
            ansi->paramCount = 0;
            return true;
        }
    }

    if (ansi->length == 2)
    {
        if (ansi->buffer[1] == '[')
        {
            return false;
        }
        else
        {
            ansi_sending_init(ansi);
            return false;
        }
    }

    if (isdigit(ansi->buffer[ansi->length - 1]))
    {
        ansi->parameters[ansi->paramCount] = ansi->parameters[ansi->paramCount] * 10 +
            (ansi->buffer[ansi->length - 1] - '0');
        return false;
    }
    else if (ansi->buffer[ansi->length - 1] == ';')
    {
        if (ansi->paramCount >= ANSI_MAX_LENGTH - 1)
        {
            ansi_sending_init(ansi);
            return false;
        }
        ansi->paramCount++;
        return false;
    }
    else
    {
        ansi->command = ansi->buffer[ansi->length - 1];
        ansi->ascii = false;
        ansi->length = 0;
        ansi->paramCount++;
        return true;
    }
}

#include "ansi.h"

#include <errno.h>
#include <string.h>

void ansi_init(ansi_t* ansi)
{
    memset(ansi, 0, sizeof(ansi_t));
}

uint64_t ansi_parse(ansi_t* ansi, char input, ansi_result_t* result)
{
    result->type = ANSI_STILL_PARSING;

    ansi->buffer[ansi->length++] = input;
    ansi->buffer[ansi->length] = '\0';
    if (ansi->length >= ANSI_MAX_LENGTH - 1)
    {
        ansi->length = 0;
        return PFAIL;
    }

    if (ansi->length == 1)
    {
        if (ansi->buffer[0] >= 32 && ansi->buffer[0] <= 126)
        {
            result->type = ANSI_PRINTABLE;
            result->printable = ansi->buffer[0];
            ansi->length = 0;
            return 0;
        }

        switch (ansi->buffer[0])
        {
        case '\033':
            return 0;
        case '\b':
            result->type = ANSI_BACKSPACE;
            ansi->length = 0;
            return 0;
        case '\n':
            result->type = ANSI_NEWLINE;
            ansi->length = 0;
            return 0;
        case '\t':
            result->type = ANSI_TAB;
            ansi->length = 0;
            return 0;
        case '\003':
            result->type = ANSI_CTRL_C;
            ansi->length = 0;
            return 0;
        default:
            ansi->length = 0;
            return PFAIL;
        }
    }

    if (ansi->length == 2)
    {
        if (ansi->buffer[1] == '[')
        {
            return 0;
        }
        else
        {
            ansi->length = 0;
            return PFAIL;
        }
    }

    if (ansi->length == 3)
    {
        if (strcmp(ansi->buffer, "\033[A") == 0)
        {
            result->type = ANSI_ARROW_UP;
            ansi->length = 0;
            return 0;
        }
        else if (strcmp(ansi->buffer, "\033[B") == 0)
        {
            result->type = ANSI_ARROW_DOWN;
            ansi->length = 0;
            return 0;
        }
        else if (strcmp(ansi->buffer, "\033[C") == 0)
        {
            result->type = ANSI_ARROW_RIGHT;
            ansi->length = 0;
            return 0;
        }
        else if (strcmp(ansi->buffer, "\033[D") == 0)
        {
            result->type = ANSI_ARROW_LEFT;
            ansi->length = 0;
            return 0;
        }
    }

    if (ansi->length == 4)
    {
        if (strcmp(ansi->buffer, "\033[3~") == 0)
        {
            result->type = ANSI_DELETE;
            ansi->length = 0;
            return 0;
        }
        else if (strcmp(ansi->buffer, "\033[5~") == 0)
        {
            result->type = ANSI_PAGE_UP;
            ansi->length = 0;
            return 0;
        }
        else if (strcmp(ansi->buffer, "\033[6~") == 0)
        {
            result->type = ANSI_PAGE_DOWN;
            ansi->length = 0;
            return 0;
        }
        else if (strcmp(ansi->buffer, "\033[7~") == 0)
        {
            result->type = ANSI_HOME;
            ansi->length = 0;
            return 0;
        }
        else if (strcmp(ansi->buffer, "\033[8~") == 0)
        {
            result->type = ANSI_END;
            ansi->length = 0;
            return 0;
        }
        // @todo Function keys?
    }

    ansi->length = 0;
    return PFAIL;
}

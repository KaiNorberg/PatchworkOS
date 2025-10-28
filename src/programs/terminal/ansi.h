#pragma once

#include <ctype.h>
#include <libpatchwork/event.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief ANSI.
 * @defgroup programs_terminal_ansi ANSI
 * @ingroup programs_terminal
 *
 * @{
 */

/**
 * @brief The size we use for buffers when parsing ANSI sequences.
 */
#define ANSI_MAX_LENGTH 8

/**
 * @brief ANSI receiving structure.
 * @struct ansi_receiving_t
 */
typedef struct
{
    char buffer[ANSI_MAX_LENGTH];
    uint8_t length;
} ansi_receiving_t;

/**
 * @brief Convert a keycode to an ANSI receiving sequence.
 *
 * A receiving sequence is a sequence sent from the terminal and received by processes running in the terminal.
 *
 * @param ansi The receiving structure to store the sequence in.
 * @param kbd The keyboard event to convert.
 */
static inline void ansi_kbd_to_receiving(ansi_receiving_t* ansi, const event_kbd_t* kbd)
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

/**
 * @brief ANSI sending structure.
 * @struct ansi_sending_t
 */
typedef struct
{
    char buffer[ANSI_MAX_LENGTH];
    uint8_t length;
    uint8_t parameters[ANSI_MAX_LENGTH];
    uint8_t paramCount;
    char command;
    bool ascii;
} ansi_sending_t;

/**
 * @brief Initialize an ANSI sending structure.
 *
 * @param ansi The sending structure to initialize.
 */
static inline void ansi_sending_init(ansi_sending_t* ansi)
{
    memset(ansi->buffer, 0, sizeof(ansi->buffer));
    ansi->length = 0;
    memset(ansi->parameters, 0, sizeof(ansi->parameters));
    ansi->paramCount = 0;
    ansi->command = '\0';
    ansi->ascii = false;
}

/**
 * @brief Parse a character for ANSI sending.
 *
 * @param ansi The sending structure.
 * @param chr The character to parse.
 * @return Whether the sequence is complete or still parsing.
 */
static inline bool ansi_sending_parse(ansi_sending_t* ansi, char chr)
{
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

    if (ansi->length >= ANSI_MAX_LENGTH - 1)
    {
        ansi_sending_init(ansi);
        return false;
    }

    if (isdigit(ansi->buffer[ansi->length - 1]))
    {
        ansi->parameters[ansi->paramCount] =
            ansi->parameters[ansi->paramCount] * 10 + (ansi->buffer[ansi->length - 1] - '0');
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

/** @} */

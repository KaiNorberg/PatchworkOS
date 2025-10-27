#pragma once

#include <libpatchwork/event.h>
#include <stdint.h>

/**
 * @brief ANSI.
 * @defgroup programs_terminal_ansi ANSI
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
void ansi_kbd_to_receiving(ansi_receiving_t* ansi, const event_kbd_t* kbd);

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
void ansi_sending_init(ansi_sending_t* ansi);

/**
 * @brief Parse a character for ANSI sending.
 *
 * @param ansi The sending structure.
 * @param chr The character to parse.
 * @return Whether the sequence is complete or still parsing.
 */
bool ansi_sending_parse(ansi_sending_t* ansi, char chr);

/** @} */

#pragma once

#include "ansi.h"

#include <libpatchwork/patchwork.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/kbd.h>
#include <threads.h>

/**
 * @brief Terminal Program.
 * @defgroup programs_terminal Terminal
 * @ingroup programs
 *
 * A simple terminal emulator program.
 *
 * The terminal always acts in raw mode, meaning that it does not process any input itself, instead it just sends all
 * input directly to the shell program running inside it.
 *
 * @see [Terminals OSDev Wiki](https://wiki.osdev.org/Terminals)
 * @see [ANSI Escape Codes](https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797)
 *
 * @{
 */

/**
 * @brief Terminal blink rate.
 */
#define TERMINAL_BLINK_INTERVAL (CLOCKS_PER_SEC / 2)

/**
 * @brief Terminal columns.
 */
#define TERMINAL_COLUMNS 80

/**
 * @brief Terminal rows.
 */
#define TERMINAL_ROWS 30

/**
 * @brief Event sent from the terminals io thread to the main thread when there is data available.
 */
#define UEVENT_TERMINAL_DATA (UEVENT_START + 0)

/**
 * @brief Maximum terminal input length.
 */
#define TERMINAL_MAX_INPUT 64

/**
 * @brief Terminal data event structure.
 * struct uevent_terminal_data_t
 *
 * The data sent from the io thread to the main thread when there is data.
 */
typedef struct
{
    char buffer[TERMINAL_MAX_INPUT];
    uint64_t length;
} uevent_terminal_data_t;

/**
 * @brief Terminal flags.
 * @enum terminal_flags_t
 *
 * Used for the ANSI state machine and character attributes.
 */
typedef enum
{
    TERMINAL_NONE = 0,
    TERMINAL_BOLD = (1 << 0),
    TERMINAL_DIM = (1 << 1),
    TERMINAL_ITALIC = (1 << 2),
    TERMINAL_UNDERLINE = (1 << 3),
    TERMINAL_BLINK = (1 << 4),
    TERMINAL_INVERSE = (1 << 5),
    TERMINAL_HIDDEN = (1 << 6),
    TERMINAL_STRIKETHROUGH = (1 << 7)
} terminal_flags_t;

/**
 * @brief Terminal character.
 * @struct terminal_char_t
 */
typedef struct
{
    char chr;
    pixel_t foreground;
    pixel_t background;
    terminal_flags_t flags;
    uint16_t col;
    uint16_t physicalRow;
} terminal_char_t;

/**
 * @brief Terminal structure.
 * @struct terminal_t
 */
typedef struct terminal
{
    window_t* win;
    font_t* font;
    bool cursorBlink;
    bool isCursorVisible;
    fd_t stdin[2];
    fd_t stdout[2]; // Also does stderr
    pixel_t foreground;
    pixel_t background;
    terminal_flags_t flags;
    ansi_sending_t ansi;
    terminal_char_t screen[TERMINAL_ROWS][TERMINAL_COLUMNS];
    uint64_t firstRow; // For scrolling
    terminal_char_t* savedCursor;
    terminal_char_t* prevCursor;
    terminal_char_t* cursor;
    pid_t shell;
} terminal_t;

/**
 * @brief Terminal initialization context.
 * @struct terminal_init_ctx_t
 *
 * Used while creating the window to pass in the font to use.
 */
typedef struct
{
    font_t* font;
} terminal_init_ctx_t;

/**
 * @brief Create a new terminal window.
 *
 * @param disp The display to create the window on.
 * @return On success, the terminal window. On failure, `NULL` and `errno` is set.
 */
window_t* terminal_new(display_t* disp);

/**
 * @brief Terminal main loop.
 *
 * @param win The terminal window.
 */
void terminal_loop(window_t* win);

/** @} */

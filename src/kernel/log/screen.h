#pragma once

#include <stdint.h>

#include <boot/boot_info.h>

#include "config.h"
#include "log/glyphs.h"
#include "utils/ring.h"

/**
 * @brief Screen logging
 * @defgroup kernel_log_screen Screen
 * @ingroup kernel_log
 *
 * @{
 */

/**
 * @brief Number of spaces to indent when a line wraps.
 */
#define SCREEN_WRAP_INDENT 4

/**
 * @brief Represents a position on the screen in character coordinates.
 */
typedef struct
{
    uint64_t x;
    uint64_t y;
} screen_pos_t;

/**
 * @brief Maximum number of characters in a single line.
 */
#define SCREEN_LINE_MAX_LENGTH (130)

/**
 * @brief The stride of a screen line in pixels.
 */
#define SCREEN_LINE_STRIDE (SCREEN_LINE_MAX_LENGTH * GLYPH_WIDTH)

/**
 * @brief A single line in the screen buffer.
 */
typedef struct
{
    uint64_t length; ///< The distance from the start of the line to the end of the furthest away char, in chars.
    uint32_t pixels[GLYPH_HEIGHT * SCREEN_LINE_STRIDE]; ///< The pixel data for the line.
} screen_line_t;

/**
 * @brief The screen buffer.
 */
typedef struct
{
    uint64_t width;            ///< The width of the buffer in chars.
    uint64_t height;           ///< The height of the buffer in chars.
    uint64_t firstLineIndex;   ///< The index of the first line in the buffer.
    screen_pos_t invalidStart; ///< The start of the invalid region in the buffer, forms a rectangle with invalidEnd.
    screen_pos_t invalidEnd;   ///< The end of the invalid region in the buffer, forms a rectangle with invalidStart.
    screen_line_t lines[CONFIG_SCREEN_MAX_LINES]; ///< The lines in the buffer, acts as a circular buffer.
} screen_buffer_t;

/**
 * @brief The screen state.
 */
typedef struct
{
    bool initialized;       ///< Whether the screen has been initialized.
    boot_gop_t gop;         ///< The GOP information.
    screen_pos_t cursor;    ///< The current cursor position in character coordinates.
    screen_buffer_t buffer; ///< The screen buffer.
} screen_t;

/**
 * @brief Initialize the screen logging.
 *
 * @param screen The screen state to initialize.
 * @param gop The boot GOP information.
 */
void screen_init(screen_t* screen, const boot_gop_t* gop);

/**
 * @brief Clear the screen and populate it with the contents of the ring buffer.
 *
 * @param screen The screen state.
 * @param ring The ring buffer to populate the screen with, can be NULL.
 */
void screen_enable(screen_t* screen, const ring_t* ring);

/**
 * @brief Disable the screen logging.
 *
 * @param screen The screen state.
 */
void screen_disable(screen_t* screen);

/**
 * @brief Write a string to the screen.
 *
 * @param screen The screen state.
 * @param string The string to write.
 * @param length The length of the string.
 */
void screen_write(screen_t* screen, const char* string, uint64_t length);

/** @} */

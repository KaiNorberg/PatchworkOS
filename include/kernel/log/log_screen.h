#pragma once

#include <kernel/config.h>
#include <kernel/log/glyphs.h>
#include <kernel/utils/ring.h>

#include <boot/boot_info.h>

#include <stdint.h>

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
#define SCREEN_WRAP_INDENT 29

/**
 * @brief Maximum number of characters in a single line.
 */
#define SCREEN_LINE_MAX_LENGTH (130)

/**
 * @brief The stride of a screen line in pixels.
 */
#define SCREEN_LINE_STRIDE (SCREEN_LINE_MAX_LENGTH * GLYPH_WIDTH)

/**
 * @brief Represents a position on the screen in character coordinates.
 */
typedef struct
{
    uint64_t x;
    uint64_t y;
} log_screen_pos_t;

/**
 * @brief A single line in the screen buffer.
 */
typedef struct
{
    uint64_t length; ///< The distance from the start of the line to the end of the furthest away char, in chars.
    uint32_t pixels[GLYPH_HEIGHT * SCREEN_LINE_STRIDE]; ///< The pixel data for the line.
} log_screen_line_t;

/**
 * @brief The screen buffer.
 */
typedef struct
{
    uint64_t width;          ///< The width of the buffer in chars.
    uint64_t height;         ///< The height of the buffer in chars.
    uint64_t firstLineIndex; ///< The index of the first line in the buffer.
    log_screen_pos_t
        invalidStart;            ///< The start of the invalid region in the buffer, forms a rectangle with invalidEnd.
    log_screen_pos_t invalidEnd; ///< The end of the invalid region in the buffer, forms a rectangle with invalidStart.
    log_screen_line_t lines[CONFIG_SCREEN_MAX_LINES]; ///< The lines in the buffer, acts as a circular buffer.
} log_screen_t;

/**
 * @brief Initialize the screen logging.
 *
 * @param screen The screen state to initialize.
 * @param bootGop Pointer to the bootloader-provided GOP information for screen logging.
 */
void log_screen_init(const boot_gop_t* bootGop);

/**
 * @brief Clear the screen.
 */
void log_screen_clear(void);

/**
 * @brief Get screen width in characters.
 */
uint64_t log_screen_get_width(void);

/**
 * @brief Get screen height in characters.
 */
uint64_t log_screen_get_height(void);

/**
 * @brief Write a string to the screen.
 *
 * @param screen The screen state.
 * @param string The string to write.
 * @param length The length of the string.
 */
void log_screen_write(const char* string, uint64_t length);

/** @} */

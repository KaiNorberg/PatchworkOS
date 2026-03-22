#pragma once

#include <kernel/config.h>
#include <kernel/log/glyphs.h>

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
 *
 * Header length (15) + 2 for indentation
 */
#define SCREEN_WRAP_INDENT 17

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
    uint8_t x;
    uint8_t y;
} screen_pos_t;

/**
 * @brief A single line in the screen buffer.
 */
typedef struct
{
    uint8_t length; ///< The distance from the start of the line to the end of the furthest away char, in chars.
    uint32_t pixels[GLYPH_HEIGHT * SCREEN_LINE_STRIDE]; ///< The pixel data for the line.
} screen_line_t;

/**
 * @brief Initialize and enable the screen logging.
 */
void screen_init(void);

/**
 * @brief Show the screen logging.
 */
void screen_show(void);

/**
 * @brief Hide the screen logging.
 */
void screen_hide(void);

/**
 * @brief Show the screen without locking, for panic situations.
 */
void screen_panic(void);

/**
 * @brief Get screen width in characters.
 */
uint64_t screen_get_width(void);

/**
 * @brief Get screen height in characters.
 */
uint64_t screen_get_height(void);

/**
 * @brief Write a string to the screen.
 *
 * @param screen The screen state.
 * @param string The string to write.
 * @param length The length of the string.
 */
void screen_write(const char* string, uint64_t length);

/** @} */

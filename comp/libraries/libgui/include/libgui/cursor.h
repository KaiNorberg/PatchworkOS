#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgfx/pixel.h>
#include <stdbool.h>

typedef struct gui_layout gui_layout_t;

/**
 * @brief Cursor definitions and helpers
 * @defgroup comp_libgui_cursor Cursor
 * @ingroup comp
 *
 * For the sake of simplicity, we use a cursor system similar to X11. Sharing both their file format and naming.
 *
 * @see [https://www.x.org/releases/current/doc/man/man3/Xcursor.3.xhtml](x.org) for information on the cursor file
 * format.
 *
 * @{
 */

/**
 * @brief Mouse cursors.
 * @enum gui_cursor_t
 */
typedef enum
{
    GUI_CURSOR_NONE, ///< Will use parents cursor or default if it has not parent.
    GUI_CURSOR_ALIAS,
    GUI_CURSOR_ALL_SCROLL,
    GUI_CURSOR_ARROW,
    GUI_CURSOR_BD_DOUBLE_ARROW,
    GUI_CURSOR_BOTTOM_LEFT_CORNER,
    GUI_CURSOR_BOTTOM_RIGHT_CORNER,
    GUI_CURSOR_BOTTOM_SIDE,
    GUI_CURSOR_BOTTOM_TEE,
    GUI_CURSOR_CELL,
    GUI_CURSOR_CENTER_PTR,
    GUI_CURSOR_CIRCLE,
    GUI_CURSOR_CLOSEDHAND,
    GUI_CURSOR_COLOR_PICKER,
    GUI_CURSOR_COL_RESIZE,
    GUI_CURSOR_CONTEXT_MENU,
    GUI_CURSOR_COPY,
    GUI_CURSOR_CROSS,
    GUI_CURSOR_CROSSED_CIRCLE,
    GUI_CURSOR_CROSSHAIR,
    GUI_CURSOR_CROSS_REVERSE,
    GUI_CURSOR_DEFAULT,
    GUI_CURSOR_DIAMOND_CROSS,
    GUI_CURSOR_DND_ASK,
    GUI_CURSOR_DND_COPY,
    GUI_CURSOR_DND_LINK,
    GUI_CURSOR_DND_MOVE,
    GUI_CURSOR_DND_NO_DROP,
    GUI_CURSOR_DND_NONE,
    GUI_CURSOR_DOTBOX,
    GUI_CURSOR_DOT_BOX_MASK,
    GUI_CURSOR_DOUBLE_ARROW,
    GUI_CURSOR_DOWN_ARROW,
    GUI_CURSOR_DRAFT,
    GUI_CURSOR_DRAFT_LARGE,
    GUI_CURSOR_DRAFT_SMALL,
    GUI_CURSOR_DRAPED_BOX,
    GUI_CURSOR_E_RESIZE,
    GUI_CURSOR_EW_RESIZE,
    GUI_CURSOR_FD_DOUBLE_ARROW,
    GUI_CURSOR_FLEUR,
    GUI_CURSOR_FORBIDDEN,
    GUI_CURSOR_GRAB,
    GUI_CURSOR_GRABBING,
    GUI_CURSOR_HAND1,
    GUI_CURSOR_HAND2,
    GUI_CURSOR_H_DOUBLE_ARROW,
    GUI_CURSOR_HELP,
    GUI_CURSOR_IBEAM,
    GUI_CURSOR_ICON,
    GUI_CURSOR_LEFT_ARROW,
    GUI_CURSOR_LEFT_PTR,
    GUI_CURSOR_LEFT_PTR_HELP,
    GUI_CURSOR_LEFT_PTR_WATCH,
    GUI_CURSOR_LEFT_SIDE,
    GUI_CURSOR_LEFT_TEE,
    GUI_CURSOR_LINK,
    GUI_CURSOR_LL_ANGLE,
    GUI_CURSOR_LR_ANGLE,
    GUI_CURSOR_MOVE,
    GUI_CURSOR_NE_RESIZE,
    GUI_CURSOR_NESW_RESIZE,
    GUI_CURSOR_NO_DROP,
    GUI_CURSOR_NOT_ALLOWED,
    GUI_CURSOR_N_RESIZE,
    GUI_CURSOR_NS_RESIZE,
    GUI_CURSOR_NW_RESIZE,
    GUI_CURSOR_NWSE_RESIZE,
    GUI_CURSOR_OPENHAND,
    GUI_CURSOR_PENCIL,
    GUI_CURSOR_PIRATE,
    GUI_CURSOR_PLUS,
    GUI_CURSOR_POINTER,
    GUI_CURSOR_POINTER_MOVE,
    GUI_CURSOR_POINTING_HAND,
    GUI_CURSOR_PROGRESS,
    GUI_CURSOR_QUESTION_ARROW,
    GUI_CURSOR_RIGHT_ARROW,
    GUI_CURSOR_RIGHT_PTR,
    GUI_CURSOR_RIGHT_SIDE,
    GUI_CURSOR_RIGHT_TEE,
    GUI_CURSOR_ROW_RESIZE,
    GUI_CURSOR_SB_DOWN_ARROW,
    GUI_CURSOR_SB_H_DOUBLE_ARROW,
    GUI_CURSOR_SB_LEFT_ARROW,
    GUI_CURSOR_SB_RIGHT_ARROW,
    GUI_CURSOR_SB_UP_ARROW,
    GUI_CURSOR_SB_V_DOUBLE_ARROW,
    GUI_CURSOR_SE_RESIZE,
    GUI_CURSOR_SIZE_ALL,
    GUI_CURSOR_SIZE_BDIAG,
    GUI_CURSOR_SIZE_FDIAG,
    GUI_CURSOR_SIZE_HOR,
    GUI_CURSOR_SIZE_VER,
    GUI_CURSOR_SPLIT_H,
    GUI_CURSOR_SPLIT_V,
    GUI_CURSOR_S_RESIZE,
    GUI_CURSOR_SW_RESIZE,
    GUI_CURSOR_TARGET,
    GUI_CURSOR_TCROSS,
    GUI_CURSOR_TEXT,
    GUI_CURSOR_TOP_LEFT_ARROW,
    GUI_CURSOR_TOP_LEFT_CORNER,
    GUI_CURSOR_TOP_RIGHT_CORNER,
    GUI_CURSOR_TOP_SIDE,
    GUI_CURSOR_TOP_TEE,
    GUI_CURSOR_UL_ANGLE,
    GUI_CURSOR_UP_ARROW,
    GUI_CURSOR_UR_ANGLE,
    GUI_CURSOR_V_DOUBLE_ARROW,
    GUI_CURSOR_VERTICAL_TEXT,
    GUI_CURSOR_WAIT,
    GUI_CURSOR_WATCH,
    GUI_CURSOR_WAYLAND_CURSOR,
    GUI_CURSOR_WHATS_THIS,
    GUI_CURSOR_W_RESIZE,
    GUI_CURSOR_X_CURSOR,
    GUI_CURSOR_XTERM,
    GUI_CURSOR_ZOOM_IN,
    GUI_CURSOR_ZOOM_OUT,
    GUI_CURSOR_MAX,
} gui_cursor_t;

/**
 * @brief Converts a cursor enum to its string representation.
 *
 * @param cursor The cursor to convert.
 * @return The string representation of the cursor.
 */
const char* gui_cursor_to_string(gui_cursor_t cursor);

/**
 * @brief Structure representing a singular frame within a cursor image.
 * @struct gui_cursor_frame_t
 */
typedef struct gui_cursor_frame
{
    uint32_t xhot;        ///< The x-coordinate where the cursor is actually pointing.
    uint32_t yhot;        ///< The y-coordinate where the cursor is actually pointing.
    uint32_t delay;       ///< The delay in milliseconds before the next frame in an animated cursor.
    gfx_pixel_t pixels[]; ///< The images pixel data.
} gui_cursor_frame_t;

/**
 * @brief Structure representing a cursor image.
 * @struct gui_cursor_image_t
 *
 * A cursor file is animated if it contains more than one image.
 */
typedef struct gui_cursor_image
{
    uint32_t width;              ///< The width of the images in pixels.
    uint32_t height;             ///< The height of the images in pixels.
    uint32_t count;              ///< The amount of frames in the animation.
    gui_cursor_frame_t** frames; ///< The frames in the animation.
} gui_cursor_image_t;

/**
 * @brief Structure representing a cursor file.
 * @struct gui_cursor_file_t
 *
 * A cursor file is animated if it contains more than one image.
 */
typedef struct gui_cursor_file
{
    uint32_t count;              ///< The amount of images in the file.
    gui_cursor_image_t** images; ///< The images in the file.
} gui_cursor_file_t;

/**
 * @brief Load a cursor file from a path.
 *
 * @param path The path to the cursor file.
 * @param out Output pointer for the loaded cursor file.
 * @return An appropriate status value.
 */
status_t gui_cursor_file_load(const char* path, gui_cursor_file_t* out);

/**
 * @brief Hidden cursor theme structure.
 * @struct gui_cursor_theme_t
 */
typedef struct gui_cursor_theme gui_cursor_theme_t;

/**
 * @brief Load the current cursor theme.
 *
 * @param out Output pointer for the created cursor theme.
 * @return An appropriate status value.
 */
status_t gui_cursor_theme_load(gui_cursor_theme_t** out);

/**
 * @brief Free a cursor theme and all its associated cursors.
 *
 * @param theme The cursor theme to free.
 */
void gui_cursor_theme_free(gui_cursor_theme_t* theme);

/**
 * @brief Get a cursor file from a theme.
 *
 * @param theme The cursor theme.
 * @param cursor The cursor type to retrieve.
 * @return The cursor file, or a default cursor if the requested cursor is not available.
 */
gui_cursor_file_t* gui_cursor_theme_get_cursor(gui_cursor_theme_t* theme, gui_cursor_t cursor);

/**
 * @brief Hidden helper structure for rendering cursors.
 * @struct gui_cursor_state_t
 */
typedef struct gui_cursor_state gui_cursor_state_t;

/**
 * @brief Create a new cursor state.
 *
 * The cursor is not guaranteed to be the desired size, just the closest match available in the theme.
 *
 * @param theme The cursor theme to use.
 * @param desiredWidth The desired width of the cursor.
 * @param desiredHeight The desired height of the cursor.
 * @param out Output pointer for the created cursor state.
 * @return An appropriate status value.
 */
status_t gui_cursor_state_new(gui_cursor_theme_t* theme, uint32_t desiredWidth, uint32_t desiredHeight,
    gui_cursor_state_t** out);

/**
 * @brief Free a cursor state.
 *
 * @param state The cursor state to free.
 */
void gui_cursor_state_free(gui_cursor_state_t* state);

/**
 * @brief Get the time until the next frame in an animated cursor.
 *
 * @param state The cursor state.
 * @param now The current time in clock ticks.
 * @return The time in clock ticks until the next frame.
 */
clock_t gui_cursor_state_next_frame(gui_cursor_state_t* state, clock_t now);

/**
 * @brief Update the cursor state, potentially advancing or resetting the animation frame.
 *
 * @param state The cursor state to update.
 * @param cursor The current cursor type.
 * @param now The current time in clock ticks.
 */
void gui_cursor_state_update(gui_cursor_state_t* state, gui_cursor_t cursor, clock_t now);

/**
 * @brief Draw the current cursor frame to a graphics context.
 *
 * @param state The cursor state.
 * @param gfx The graphics context to draw to.
 * @param x The x-coordinate to draw the cursor at.
 * @param y The y-coordinate to draw the cursor at.
 */
void gui_cursor_state_draw(gui_cursor_state_t* state, gfx_t* gfx, int32_t x, int32_t y);

/**
 * @brief Clear a previously drawn cursor from a graphics context.

 * @param state The cursor state.
 * @param gfx The graphics context to clear from.
 */
void gui_cursor_state_clear(gui_cursor_state_t* state, gfx_t* gfx);

/**
 * @brief Get the bounding box of the current cursor frame.
 *
 * @param state The cursor state.
 * @return The bounds of the current cursor frame.
 */
gfx_rect_t gui_cursor_state_get_bounds(gui_cursor_state_t* state);

/** @} */
#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgfx/rect.h>
#include <stdbool.h>

/**
 * @brief Visual Formatting
 * @defgroup comp_libgui_format Formatting
 * @ingroup comp
 *
 * The GUI library uses a formatting system inspired by the visual formatting of CSS, heavily leaning on their Box Model
 * and "Flexible Box" display system.
 *
 * As such, the system described below ought to be familiar to anyone unfortunate enough to have worked on web based
 * frontends.
 *
 * @see [https://www.w3.org/TR/css-2026/#fairly-stable](W3 CSS Snapshot 2026)
 *
 * ## Box Model
 *
 * All widgets are represented as rectangular boxes. The box model consists of several layers, each layer containing the
 * next:
 *
 * - **Margin**: A transparent outer layer, providing space between the widget and its neighbors, only the margin can be
 * a negative size.
 * - **Border**: A drawn region around the padding and content.
 * - **Padding**: Space between the border and the content, is still considered part of the widget's background.
 * - **Content**: The actual content of the widget (e.g., text, image, or child widgets).
 *
 * @see [https://www.w3.org/TR/css-box-3/](W3 CSS Box 3)
 *
 * @{
 */

/**
 * @brief Represents spacing for CSS-like margins and paddings.
 * @struct gui_spacing_t
 *
 * @note For any member within a spacing structure, the "positive" direction is defined as "outwards" from the content
 * box.
 */
typedef struct gui_spacing
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} gui_spacing_t;

/**
 * @brief Helper macro to create a `gui_spacing_t` structure.
 *
 * @param _l Left spacing.
 * @param _t Top spacing.
 * @param _r Right spacing.
 * @param _b Bottom spacing.
 */
#define GUI_SPACING(_l, _t, _r, _b) \
    (gui_spacing_t) \
    { \
        .left = (_l), .top = (_t), .right = (_r), .bottom = (_b) \
    }

/**
 * @brief Helper macro to create a `gui_spacing_t` structure with the same value for all sides.
 *
 * @param _v The value for all sides.
 */
#define GUI_SPACING_ALL(_v) GUI_SPACING(_v, _v, _v, _v)

/**
 * @brief Helper macro to create a `gui_spacing_t` structure with symmetric horizontal and vertical spacing.
 *
 * @param _h Horizontal spacing (left and right).
 * @param _v Vertical spacing (top and bottom).
 */
#define GUI_SPACING_HV(_h, _v) GUI_SPACING(_h, _v, _h, _v)

/**
 * @brief Helper macro to create a `gui_spacing_t` structure with all sides set to zero.
 */
#define GUI_SPACING_INITIAL() GUI_SPACING(0, 0, 0, 0)

/**
 * @brief Display modes for widgets.
 * @enum gui_display_t
 *
 * @see [https://www.w3.org/TR/css-display-4](W3 CSS Display 4)
 */
typedef enum gui_display
{
    GUI_DISPLAY_NONE,
    GUI_DISPLAY_FLEX,
    GUI_DISPLAY_GRID,
    GUI_DISPLAY_INITIAL = GUI_DISPLAY_NONE,
} gui_display_t;

/**
 * @brief Flex direction for flexbox layout.
 * @enum gui_flex_direction_t
 *
 * @see [https://www.w3.org/TR/css-flexbox-1/#flex-direction-property](W3 CSS Flexbox 1 Flex Direction Property)
 */
typedef enum gui_flex_direction
{
    GUI_FLEX_DIRECTION_ROW,
    GUI_FLEX_DIRECTION_ROW_REVERSE,
    GUI_FLEX_DIRECTION_COLUMN,
    GUI_FLEX_DIRECTION_COLUMN_REVERSE,
    GUI_FLEX_DIRECTION_INITIAL = GUI_FLEX_DIRECTION_ROW,
} gui_flex_direction_t;

/**
 * @brief Flex wrap for flexbox layout.
 * @enum gui_flex_wrap_t
 *
 * @see [https://www.w3.org/TR/css-flexbox-1/#flex-wrap-property](W3 CSS Flexbox 1 Flex Wrap Property)
 */
typedef enum gui_flex_wrap
{
    GUI_FLEX_WRAP_NOWRAP,
    GUI_FLEX_WRAP_WRAP,
    GUI_FLEX_WRAP_WRAP_REVERSE,
    GUI_FLEX_WRAP_INITIAL = GUI_FLEX_WRAP_NOWRAP,
} gui_flex_wrap_t;

/**
 * @brief Justify content enum.
 * @enum gui_justify_content_t
 *
 * @see [https://www.w3.org/TR/css-flexbox-1/#justify-content-property](W3 CSS Flexbox 1 Justify Content Property)
 */
typedef enum gui_justify_content
{
    GUI_JUSTIFY_CONTENT_FLEX_START,
    GUI_JUSTIFY_CONTENT_FLEX_END,
    GUI_JUSTIFY_CONTENT_CENTER,
    GUI_JUSTIFY_CONTENT_SPACE_BETWEEN,
    GUI_JUSTIFY_CONTENT_SPACE_AROUND,
    GUI_JUSTIFY_CONTENT_INITIAL = GUI_JUSTIFY_CONTENT_FLEX_START,
} gui_justify_content_t;

/**
 * @brief Align items enum.
 * @enum gui_align_items_t
 *
 * @see [https://www.w3.org/TR/css-flexbox-1/#align-items-property](W3 CSS Flexbox 1 Align Items Property)
 */
typedef enum gui_align_items
{
    GUI_ALIGN_ITEMS_FLEX_START,
    GUI_ALIGN_ITEMS_FLEX_END,
    GUI_ALIGN_ITEMS_CENTER,
    GUI_ALIGN_ITEMS_BASELINE,
    GUI_ALIGN_ITEMS_STRETCH,
    GUI_ALIGN_ITEMS_INITIAL = GUI_ALIGN_ITEMS_STRETCH,
} gui_align_items_t;

/**
 * @brief Align self enum.
 * @enum gui_align_self_t
 *
 * @see [https://www.w3.org/TR/css-flexbox-1/#align-self-property](W3 CSS Flexbox 1 Align Self Property)
 */
typedef enum gui_align_self
{
    GUI_ALIGN_SELF_AUTO,
    GUI_ALIGN_SELF_FLEX_START,
    GUI_ALIGN_SELF_FLEX_END,
    GUI_ALIGN_SELF_CENTER,
    GUI_ALIGN_SELF_BASELINE,
    GUI_ALIGN_SELF_STRETCH,
    GUI_ALIGN_SELF_INITIAL = GUI_ALIGN_SELF_AUTO,
} gui_align_self_t;

/**
 * @brief Align content enum.
 * @enum gui_align_content_t
 *
 * @see [https://www.w3.org/TR/css-flexbox-1/#align-content-property](W3 CSS Flexbox 1 Align Content Property)
 */
typedef enum gui_align_content
{
    GUI_ALIGN_CONTENT_FLEX_START,
    GUI_ALIGN_CONTENT_FLEX_END,
    GUI_ALIGN_CONTENT_CENTER,
    GUI_ALIGN_CONTENT_SPACE_BETWEEN,
    GUI_ALIGN_CONTENT_SPACE_AROUND,
    GUI_ALIGN_CONTENT_STRETCH,
    GUI_ALIGN_CONTENT_INITIAL = GUI_ALIGN_CONTENT_STRETCH,
} gui_align_content_t;

/**
 * @brief Flex property type.
 * @typedef gui_flex_t
 *
 * @see [https://www.w3.org/TR/css-flexbox-1/#flex-grow-property](W3 CSS Flexbox 1 Flex Grow Property)
 * @see [https://www.w3.org/TR/css-flexbox-1/#flex-shrink-property](W3 CSS Flexbox 1 Flex Shrink Property)
 * @see [https://www.w3.org/TR/css-flexbox-1/#flex-basis-property](W3 CSS Flexbox 1 Flex Basis Property)
 */
typedef int32_t gui_flex_t;

#define GUI_FLEX_GROW_INITIAL 0   ///< Initial value for the flex grow property.
#define GUI_FLEX_SHRINK_INITIAL 1 ///< Initial value for the flex shrink property.
#define GUI_FLEX_BASIS_INITIAL 0  ///< Initial value for the flex basis property.

/**
 * @brief Widget formatting state.
 * @struct gui_format_t
 */
typedef struct gui_format
{
    gfx_rect_t content;
    gui_spacing_t margin;
    gui_spacing_t border;
    gui_spacing_t padding;
    gui_display_t display;
    gui_flex_direction_t flexDirection;
    gui_flex_wrap_t flexWrap;
    gui_justify_content_t justifyContent;
    gui_align_items_t alignItems;
    gui_align_content_t alignContent;
    gui_align_self_t alignSelf;
    gui_flex_t flexGrow;
    gui_flex_t flexShrink;
    gui_flex_t flexBasis;
} gui_format_t;

/**
 * @brief Helper macro to initialize a `gui_format_t` structure with initial values.
 */
#define GUI_FORMAT_INITIAL() \
    (gui_format_t) \
    { \
        .content = {0, 0, 0, 0}, .margin = GUI_SPACING_INITIAL(), .border = GUI_SPACING_INITIAL(), \
        .padding = GUI_SPACING_INITIAL(), .display = GUI_DISPLAY_INITIAL, .flexDirection = GUI_FLEX_DIRECTION_INITIAL, \
        .flexWrap = GUI_FLEX_WRAP_INITIAL, .justifyContent = GUI_JUSTIFY_CONTENT_INITIAL, \
        .alignItems = GUI_ALIGN_ITEMS_INITIAL, .alignContent = GUI_ALIGN_CONTENT_INITIAL, \
        .alignSelf = GUI_ALIGN_SELF_INITIAL, .flexGrow = GUI_FLEX_GROW_INITIAL, .flexShrink = GUI_FLEX_SHRINK_INITIAL, \
        .flexBasis = GUI_FLEX_BASIS_INITIAL \
    }

/**
 * @brief Get the content box of a widget format.
 *
 * @param format The formatting state.
 * @return The rectangle representing the content box.
 */
static inline gfx_rect_t gui_format_get_content_box(const gui_format_t* format)
{
    return format->content;
}

/**
 * @brief Get the padding box of a widget format.
 *
 * @param format The formatting state.
 * @return The rectangle representing the padding box.
 */
static inline gfx_rect_t gui_format_get_padding_box(const gui_format_t* format)
{
    gfx_rect_t content = gui_format_get_content_box(format);
    content.left -= format->padding.left;
    content.top -= format->padding.top;
    content.right += format->padding.right;
    content.bottom += format->padding.bottom;
    return content;
}

/**
 * @brief Get the border box of a widget format.
 *
 * @param format The formatting state.
 * @return The rectangle representing the border box.
 */
static inline gfx_rect_t gui_format_get_border_box(const gui_format_t* format)
{
    gfx_rect_t padding = gui_format_get_padding_box(format);
    padding.left -= format->border.left;
    padding.top -= format->border.top;
    padding.right += format->border.right;
    padding.bottom += format->border.bottom;
    return padding;
}

/**
 * @brief Get the margin box of a widget format.
 *
 * @param format The formatting state.
 * @return The rectangle representing the margin box.
 */
static inline gfx_rect_t gui_format_get_margin_box(const gui_format_t* format)
{
    gfx_rect_t border = gui_format_get_border_box(format);
    border.left -= format->margin.left;
    border.top -= format->margin.top;
    border.right += format->margin.right;
    border.bottom += format->margin.bottom;
    return border;
}

/** @} */
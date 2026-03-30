#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libdraw/draw.h>
#include <libdraw/pixel.h>
#include <libdraw/polygon.h>
#include <libdraw/rect.h>
#include <libdraw/vertices.h>
#include <stdbool.h>

typedef struct widget_layout widget_layout_t;

/**
 * @brief Generic UI widget library
 * @defgroup comp_libwidget libwidget
 * @ingroup comp
 *
 * @todo The widget library is currently unimplemented, its more of a design document.
 *
 * @{
 */

/**
 * @brief Hidden widget structure.
 * @struct widget_t
 */
typedef struct widget widget_t;

/**
 * @brief Widget ID type.
 *
 * Primarily used to specify the source of certain events.
 */
typedef uint32_t widget_id_t;

/**
 * @brief Widget mouse buttons.
 * @enum widget_mouse_buttons_t
 */
typedef enum
{
    WIDGET_MOUSE_BUTTON_NONE = 0,
    WIDGET_MOUSE_BUTTON_LEFT = (1 << 0),
    WIDGET_MOUSE_BUTTON_RIGHT = (1 << 1),
    WIDGET_MOUSE_BUTTON_MIDDLE = (1 << 2),
} widget_mouse_buttons_t;

/**
 * @brief Widget keyboard modifiers.
 * @enum widget_key_mods_t
 */
typedef enum
{
    WIDGET_KEY_MOD_NONE = 0,
    WIDGET_KEY_MOD_SHIFT = (1 << 0),
    WIDGET_KEY_MOD_CTRL = (1 << 1),
    WIDGET_KEY_MOD_ALT = (1 << 2),
    WIDGET_KEY_MOD_SUPER = (1 << 3),
    WIDGET_KEY_MOD_CAPS_LOCK = (1 << 4),
    WIDGET_KEY_MOD_NUM_LOCK = (1 << 5),
    WIDGET_KEY_MOD_SCROLL_LOCK = (1 << 6),
} widget_key_mods_t;

/**
 * @brief Widget key states.
 * @enum widget_key_state_t
 */
typedef enum
{
    WIDGET_KEY_STATE_PRESSED,
    WIDGET_KEY_STATE_RELEASED,
    WIDGET_KEY_STATE_HELD,
} widget_key_state_t;

/**
 * @brief Widget cursors.
 * @enum widget_cursor_t
 *
 * @see https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/Properties/cursor
 */
typedef enum
{
    WIDGET_CURSOR_NONE,
    WIDGET_CURSOR_DEFAULT,
    WIDGET_CURSOR_CONTEXT_MENU,
    WIDGET_CURSOR_HELP,
    WIDGET_CURSOR_POINTER,
    WIDGET_CURSOR_PROGRESS,
    WIDGET_CURSOR_WAIT,
    WIDGET_CURSOR_CELL,
    WIDGET_CURSOR_CROSSHAIR,
    WIDGET_CURSOR_TEXT,
    WIDGET_CURSOR_VERTICAL_TEXT,
    WIDGET_CURSOR_ALIAS,
    WIDGET_CURSOR_COPY,
    WIDGET_CURSOR_MOVE,
    WIDGET_CURSOR_NO_DROP,
    WIDGET_CURSOR_NOT_ALLOWED,
    WIDGET_CURSOR_GRAB,
    WIDGET_CURSOR_GRABBING,
    WIDGET_CURSOR_ALL_SCROLL,
    WIDGET_CURSOR_COL_RESIZE,
    WIDGET_CURSOR_ROW_RESIZE,
    WIDGET_CURSOR_N_RESIZE,
    WIDGET_CURSOR_E_RESIZE,
    WIDGET_CURSOR_S_RESIZE,
    WIDGET_CURSOR_W_RESIZE,
    WIDGET_CURSOR_NE_RESIZE,
    WIDGET_CURSOR_NW_RESIZE,
    WIDGET_CURSOR_SE_RESIZE,
    WIDGET_CURSOR_SW_RESIZE,
    WIDGET_CURSOR_EW_RESIZE,
    WIDGET_CURSOR_NS_RESIZE,
    WIDGET_CURSOR_NESW_RESIZE,
    WIDGET_CURSOR_NWSE_RESIZE,
    WIDGET_CURSOR_ZOOM_IN,
    WIDGET_CURSOR_ZOOM_OUT,
} widget_cursor_t;

/**
 * @brief Widget event types.
 * @enum widget_event_type_t
 */
typedef enum
{
    WIDGET_EVENT_TYPE_MOUSE,
    WIDGET_EVENT_TYPE_KEY,
    WIDGET_EVENT_TYPE_MOVE,
    WIDGET_EVENT_TYPE_DRAW,
    WIDGET_EVENT_TYPE_FOCUS_IN,
    WIDGET_EVENT_TYPE_FOCUS_OUT,
    WIDGET_EVENT_TYPE_MOUSE_ENTER,
    WIDGET_EVENT_TYPE_MOUSE_LEAVE,
} widget_event_type_t;

/**
 * @brief Widget event flags.
 * @enum widget_event_flags_t
 */
typedef enum
{
    WIDGET_EVENT_FLAG_NONE = 0,
    WIDGET_EVENT_FLAG_PROPAGATE = (1 << 0),
} widget_event_flags_t;

/**
 * @brief Widget event.
 * @struct widget_event_t
 */
typedef struct
{
    widget_event_type_t type;
    widget_event_flags_t flags;
    union {
        struct
        {
            int32_t x;
            int32_t y;
            widget_mouse_buttons_t pressed;
            widget_mouse_buttons_t released;
            widget_mouse_buttons_t held;
        } mouse;
        struct
        {
            keycode_t key;
            widget_key_state_t state;
            widget_key_mods_t modifiers;
        } key;
        struct
        {
            rect_t bounds;
        } move;
        uint64_t _reserved[7];
    };
} widget_event_t;

/**
 * @brief Widget class structure.
 * @struct widget_class_t
 */
typedef struct
{
    size_t size; ///< Size of the widget structure for this class.
    /**
     * @brief Initialize the widget.
     *
     * @param widget The widget to initialize.
     * @return An appropriate status value.
     */
    status_t (*init)(widget_t* widget);
    /**
     * @brief Deinitialize the widget.
     *
     * @param widget The widget to deinitialize.
     */
    void (*deinit)(widget_t* widget);
    /**
     * @brief Calculate the desired size of the widget based on available space.
     *
     * @param widget The widget to measure.
     * @param availableWidth The maximum available width, or `SIZE_MAX` for unconstrained.
     * @param availableHeight The maximum available height, or `SIZE_MAX` for unconstrained.
     * @return An appropriate status value.
     */
    status_t (*measure)(widget_t* widget, size_t availableWidth, size_t availableHeight);
    /**
     * @brief Handle an event for the widget.
     *
     * @param widget The widget.
     * @param event The event to handle.
     * @return An appropriate status value.
     */
    status_t (*procedure)(widget_t* widget, widget_event_t* event);
} widget_class_t;

/**
 * @brief Widget layout structure.
 * @struct widget_layout_t
 *
 * Provides policy for how a parent should arrange its children.
 */
typedef struct widget_layout
{
    /**
     * @brief Arrange the children of a widget.
     *
     * @param widget The parent widget whose children should be arranged.
     * @return An appropriate status value.
     */
    status_t (*arrange)(widget_layout_t* layout, widget_t* widget);
    void* data; ///< Optional private data.
} widget_layout_t;

/**
 * @brief Widget options.
 * @struct widget_opts_t
 *
 * @see [Freedesktop Icon Naming](https://specifications.freedesktop.org/icon-naming/latest/) for icon names.
 */
/*typedef struct
{
    const char* label; ///< The initial label text for the widget, can be `NULL`.
    const char* icon; ///< The name of the icon to display, for example "icon-close", can be `NULL`.
    void* userdata; ///< Optional application-specific data.
    uint8_t _reserved[256 - (sizeof(void*) * 3)];
} widget_opts_t;

#ifdef static_assert
static_assert(sizeof(widget_opts_t) == 256, "widget_opts_t is not 256 bytes");
#endif*/

/**
 * @brief The widget class for a canvas.
 *
 * A canvas widget is a simple empty buffer that can be used for custom drawing.
 */
#define WIDGET_CLASS_CANVAS (&widgetClassCanvas)

/**
 * @brief The widget class for a button.
 */
#define WIDGET_CLASS_BUTTON (&widgetClassButton)

/**
 * @brief The widget class for a label.
 */
#define WIDGET_CLASS_LABEL (&widgetClassLabel)

/**
 * @brief The widget class for a text input.
 */
#define WIDGET_CLASS_TEXT_INPUT (&widgetClassTextInput)

/**
 * @brief The widget class for an image.
 */
#define WIDGET_CLASS_IMAGE (&widgetClassIcon)

/**
 * @brief Create a new generic widget.
 *
 * If no parent is specified then the widget will be created as a root widget with its own separate context, tracking
 * things such as focused and hovered widgets.
 *
 * @param class The class of the widget to create.
 * @param parent The parent widget, can be `NULL`.
 * @param id The ID of the widget, used to report the source of events.
 * @param bounds The bounds of the widget relative to its parent.
 * @param out Output pointer for the created widget.
 * @return An appropriate status value.
 */
status_t widget_new(widget_class_t* class, widget_t* parent, widget_id_t id, rect_t bounds, widget_t** out);

/**
 * @brief Free a widget and all its children.
 *
 * @param widget The widget to free.
 */
void widget_free(widget_t* widget);

/**
 * @brief Invalidate a specific area of a widget, forcing it to be redrawn.
 *
 * @param widget The widget to invalidate.
 * @param area The area to invalidate, relative to the widget's origin.
 */
void widget_invalidate(widget_t* widget, rect_t area);

/**
 * @brief Force a layout recalculation starting from this widget.
 *
 * @param widget The widget.
 */
void widget_invalidate_layout(widget_t* widget);

/**
 * @brief Get the drawing context for a widget.
 *
 * @param widget The widget.
 * @return The drawing context.
 */
drawable_t* widget_get_drawable(widget_t* widget);

/**
 * @brief Get the bounds of a widget.
 *
 * @param widget The widget.
 * @return The bounds of the widget.
 */
rect_t widget_get_bounds(widget_t* widget);

/**
 * @brief Set the bounds of a widget.
 *
 * @param widget The widget.
 * @param bounds The new bounds.
 */
void widget_set_bounds(widget_t* widget, rect_t bounds);

/**
 * @brief Get the parent of a widget.
 *
 * @param widget The widget.
 * @return The parent widget, or `NULL` if it has no parent.
 */
widget_t* widget_get_parent(widget_t* widget);

/**
 * @brief Set the parent of a widget.
 *
 * @param widget The widget.
 * @param parent The new parent widget, or `NULL` to detach.
 */
void widget_set_parent(widget_t* widget, widget_t* parent);

/**
 * @brief Get the ID of a widget.
 *
 * @param widget The widget.
 * @return The ID of the widget.
 */
widget_id_t widget_get_id(widget_t* widget);

/**
 * @brief Set the ID of a widget.
 *
 * @param widget The widget.
 * @param id The new ID.
 */
void widget_set_id(widget_t* widget, widget_id_t id);

/**
 * @brief Get the label of a widget.
 *
 * @param widget The widget.
 * @return The label text, or `NULL` if it has no label.
 */
const char* widget_get_label(widget_t* widget);

/**
 * @brief Set the label of a widget.
 *
 * @param widget The widget.
 * @param label The new label text, or `NULL` to clear.
 */
void widget_set_label(widget_t* widget, const char* label);

/**
 * @brief Get the icon name of a widget.
 *
 * @param widget The widget.
 * @return The icon name, or `NULL` if it has no icon.
 */
const char* widget_get_icon(widget_t* widget);

/**
 * @brief Set the icon of a widget.
 *
 * @param widget The widget.
 * @param icon The new icon name, or `NULL` to clear.
 */
void widget_set_icon(widget_t* widget, const char* icon);

/**
 * @brief Get the cursor for a widget.
 *
 * @param widget The widget.
 * @return The cursor type.
 */
widget_cursor_t widget_get_cursor(widget_t* widget);

/**
 * @brief Set the cursor for a widget.
 *
 * @param widget The widget.
 * @param cursor The new cursor type.
 */
void widget_set_cursor(widget_t* widget, widget_cursor_t cursor);

/**
 * @brief Get the custom user data associated with a widget.
 *
 * @param widget The widget.
 * @return The user data pointer.
 */
void* widget_get_userdata(widget_t* widget);

/**
 * @brief Set custom user data for a widget.
 *
 * @param widget The widget.
 * @param userdata The user data pointer.
 */
void widget_set_userdata(widget_t* widget, void* userdata);

/** @} */

extern const widget_class_t widgetClassCanvas;

extern const widget_class_t widgetClassButton;

extern const widget_class_t widgetClassLabel;

extern const widget_class_t widgetClassTextInput;

extern const widget_class_t widgetClassIcon;
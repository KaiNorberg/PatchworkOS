#pragma once

#include <assert.h>
#include <libc/kbd.h>
#include <libc/status.h>
#include <libgfx/gfx.h>
#include <libgui/button.h>
#include <libgui/desktop.h>
#include <stdbool.h>

typedef struct gui_layout gui_layout_t;

/**
 * @brief Generic GUI widget library
 * @defgroup comp_libgui libgui
 * @ingroup comp
 *
 * The gui library provides a generic widget infrastructure for building graphical user interfaces.
 * 
 * ## Widgets
 * 
 * A interface is made up of a hierarchical tree of widgets (`gui_widget_t`), each widget is a member of a specific class (`gui_class_t`) that defines its behavior, such as how it is initialized, measured, and how it handles events.
 * 
 * All widgets can be passed to the `gui_widget_*` functions, however specific widget types such as `gui_button_t` can also be passed to `gui_button_*` functions, these functions will check the widget class before performing the operation. This pattern continues for all widget classes.
 * 
 * ## Events
 * 
 * Widgets communicate and react to user input through events (`gui_event_t`). Events include mouse movement, keyboard input, focus changes, etc.
 * 
 * One important event type is `GUI_EVENT_TYPE_COMMAND` which certain widgets will send to their parents to notify them of high-level actions (e.g., a button being clicked).
 * 
 * ## Layout
 * 
 * The positioning of widgets can be managed manually or through a layout policy (`gui_layout_t`). A layout defines how a parent widget should automatically arrange its children based on their desired measurements.
 * 
 * ## Icons
 * 
 * Widgets can optionally display an icon. Icon names should follow the [Freedesktop Icon Naming Specification](https://specifications.freedesktop.org/icon-naming/latest/).
 * 
 * @todo The widget library is currently unimplemented, its more of a design document.
 *
 * @{
 */

/**
 * @brief Hidden widget structure.
 * @struct gui_widget_t
 */
typedef struct gui_widget gui_widget_t;

/**
 * @brief Widget ID type.
 *
 * Primarily used to specify the source of certain events.
 */
typedef uint32_t gui_widget_id_t;

/**
 * @brief Widget mouse buttons.
 * @enum gui_mouse_buttons_t
 */
typedef enum
{
    GUI_MOUSE_BUTTON_NONE = 0,
    GUI_MOUSE_BUTTON_LEFT = (1 << 0),
    GUI_MOUSE_BUTTON_RIGHT = (1 << 1),
    GUI_MOUSE_BUTTON_MIDDLE = (1 << 2),
} gui_mouse_buttons_t;

/**
 * @brief Widget keyboard modifiers.
 * @enum gui_key_mods_t
 */
typedef enum
{
    GUI_KEY_MOD_NONE = 0,
    GUI_KEY_MOD_SHIFT = (1 << 0),
    GUI_KEY_MOD_CTRL = (1 << 1),
    GUI_KEY_MOD_ALT = (1 << 2),
    GUI_KEY_MOD_SUPER = (1 << 3),
    GUI_KEY_MOD_CAPS_LOCK = (1 << 4),
    GUI_KEY_MOD_NUM_LOCK = (1 << 5),
    GUI_KEY_MOD_SCROLL_LOCK = (1 << 6),
} gui_key_mods_t;

/**
 * @brief Widget key states.
 * @enum gui_key_state_t
 */
typedef enum
{
    GUI_KEY_STATE_PRESSED,
    GUI_KEY_STATE_RELEASED,
    GUI_KEY_STATE_HELD,
} gui_key_state_t;

/**
 * @brief Widget cursors.
 * @enum gui_cursor_t
 *
 * @see https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/Properties/cursor
 */
typedef enum
{
    GUI_CURSOR_NONE, ///< Will use parents cursor or default if it has not parent.
    GUI_CURSOR_DEFAULT,
    GUI_CURSOR_CONTEXT_MENU,
    GUI_CURSOR_HELP,
    GUI_CURSOR_POINTER,
    GUI_CURSOR_PROGRESS,
    GUI_CURSOR_WAIT,
    GUI_CURSOR_CELL,
    GUI_CURSOR_CROSSHAIR,
    GUI_CURSOR_TEXT,
    GUI_CURSOR_VERTICAL_TEXT,
    GUI_CURSOR_ALIAS,
    GUI_CURSOR_COPY,
    GUI_CURSOR_MOVE,
    GUI_CURSOR_NO_DROP,
    GUI_CURSOR_NOT_ALLOWED,
    GUI_CURSOR_GRAB,
    GUI_CURSOR_GRABBING,
    GUI_CURSOR_ALL_SCROLL,
    GUI_CURSOR_COL_RESIZE,
    GUI_CURSOR_ROW_RESIZE,
    GUI_CURSOR_N_RESIZE,
    GUI_CURSOR_E_RESIZE,
    GUI_CURSOR_S_RESIZE,
    GUI_CURSOR_W_RESIZE,
    GUI_CURSOR_NE_RESIZE,
    GUI_CURSOR_NW_RESIZE,
    GUI_CURSOR_SE_RESIZE,
    GUI_CURSOR_SW_RESIZE,
    GUI_CURSOR_EW_RESIZE,
    GUI_CURSOR_NS_RESIZE,
    GUI_CURSOR_NESW_RESIZE,
    GUI_CURSOR_NWSE_RESIZE,
    GUI_CURSOR_ZOOM_IN,
    GUI_CURSOR_ZOOM_OUT,
} gui_cursor_t;

/**
 * @brief Widget event types.
 * @enum gui_event_type_t
 */
typedef enum
{
    GUI_EVENT_TYPE_MOUSE,
    GUI_EVENT_TYPE_KEY,
    GUI_EVENT_TYPE_MOVE,
    GUI_EVENT_TYPE_COMMAND,
    GUI_EVENT_TYPE_FOCUS_IN,
    GUI_EVENT_TYPE_FOCUS_OUT,
    GUI_EVENT_TYPE_MOUSE_ENTER,
    GUI_EVENT_TYPE_MOUSE_LEAVE,
} gui_event_type_t;

/**
 * @brief Widget event flags.
 * @enum gui_event_flags_t
 */
typedef enum
{
    GUI_EVENT_FLAG_NONE = 0,
    GUI_EVENT_FLAG_PROPAGATE = (1 << 0),
} gui_event_flags_t;

typedef enum
{
    GUI_COMMAND_CLICK,
    GUI_COMMAND_DOUBLE_CLICK,
    GUI_COMMAND_VALUE_CHANGED,
    GUI_COMMAND_TEXT_CHANGED,
    GUI_COMMAND_SELECT,
} gui_command_t;

/**
 * @brief Widget event.
 * @struct gui_event_t
 */
typedef struct
{
    gui_event_type_t type;
    gui_event_flags_t flags;
    union {
        struct
        {
            int32_t x;
            int32_t y;
            gui_mouse_buttons_t pressed;
            gui_mouse_buttons_t released;
            gui_mouse_buttons_t held;
        } mouse;
        struct
        {
            keycode_t key;
            gui_key_state_t state;
            gui_key_mods_t modifiers;
        } key;
        struct
        {
            gfx_rect_t bounds;
        } move;
        struct
        {
            gui_widget_id_t source;
            gui_command_t command;
        } command;
        uint64_t _reserved[7];
    };
} gui_event_t;

/**
 * @brief Widget class structure.
 * @struct gui_class_t
 */
typedef struct gui_class
{
    size_t size; ///< Size of the widget structure for this class.
    /**
     * @brief Initialize the widget.
     *
     * @param widget The widget to initialize.
     * @return An appropriate status value.
     */
    status_t (*init)(gui_widget_t* widget);
    /**
     * @brief Deinitialize the widget.
     *
     * @param widget The widget to deinitialize.
     */
    void (*deinit)(gui_widget_t* widget);
    /**
     * @brief Calculate the desired size of the widget based on available space.
     *
     * @param widget The widget to measure.
     * @param availableWidth The maximum available width, or `SIZE_MAX` for unconstrained.
     * @param availableHeight The maximum available height, or `SIZE_MAX` for unconstrained.
     * @return An appropriate status value.
     */
    status_t (*measure)(gui_widget_t* widget, size_t availableWidth, size_t availableHeight);
    /**
     * @brief Handle an event for the widget.
     *
     * @param widget The widget.
     * @param event The event to handle.
     * @return An appropriate status value.
     */
    status_t (*procedure)(gui_widget_t* widget, gui_event_t* event);
} gui_class_t;

/**
 * @brief Widget layout structure.
 * @struct gui_layout_t
 *
 * Provides policy for how a parent should arrange its children.
 */
typedef struct gui_layout
{
    /**
     * @brief Arrange the children of a widget.
     *
     * @param widget The parent widget whose children should be arranged.
     * @return An appropriate status value.
     */
    status_t (*arrange)(gui_layout_t* layout, gui_widget_t* widget);
    void* data; ///< Optional private data.
} gui_layout_t;

/**
 * @brief Create a new generic widget.
 *
 * If no parent is specified then the widget will be created as a root widget with its own separate context, tracking
 * things such as focused and hovered widgets.
 *
 * The created widget will be hidden by default, use `gui_widget_show` to make it visible.
 * 
 * @param cls The class of the widget to create.
 * @param parent The parent widget, can be `NULL`.
 * @param id The ID of the widget, used to report the source of events.
 * @param bounds The bounds of the widget relative to its parent.
 * @param userdata Initial userdata for the widget, can be `NULL`.
 * @param out Output pointer for the created widget, can be `NULL`.
 * @return An appropriate status value.
 */
status_t gui_widget_new(gui_class_t* cls, gui_widget_t* parent, gui_widget_id_t id, gfx_rect_t bounds, void* userdata, gui_widget_t** out);

/**
 * @brief Free a widget and all its children.
 *
 * @param widget The widget to free.
 */
void gui_widget_free(gui_widget_t* widget);

/**
 * @brief Show a widget, making it visible.
 *
 * @param widget The widget to show.
 */
void gui_widget_show(gui_widget_t* widget);

/**
 * @brief Hide a widget, making it invisible.
 *
 * @param widget The widget to hide.
 */
void gui_widget_hide(gui_widget_t* widget);

/**
 * @brief Invalidate a specific area of a widget, forcing it to be redrawn.
 *
 * @param widget The widget to invalidate.
 * @param area The area to invalidate, relative to the widget's origin.
 */
void gui_widget_invalidate(gui_widget_t* widget, gfx_rect_t area);

/**
 * @brief Force a layout recalculation starting from this widget.
 *
 * @param widget The widget.
 */
void gui_widget_invalidate_layout(gui_widget_t* widget);

/**
 * @brief Get the drawing context for a widget.
 *
 * @param widget The widget.
 * @return The drawing context.
 */
gfx_t* gui_widget_get_gfx(gui_widget_t* widget);

/**
 * @brief Get the bounds of a widget.
 *
 * @param widget The widget.
 * @return The bounds of the widget.
 */
gfx_rect_t gui_widget_get_bounds(gui_widget_t* widget);

/**
 * @brief Set the bounds of a widget.
 *
 * @param widget The widget.
 * @param bounds The new bounds.
 * @return An appropriate status value.
 */
status_t gui_widget_set_bounds(gui_widget_t* widget, gfx_rect_t bounds);

/**
 * @brief Get the parent of a widget.
 *
 * @param widget The widget.
 * @return The parent widget, or `NULL` if it has no parent.
 */
gui_widget_t* gui_widget_get_parent(gui_widget_t* widget);

/**
 * @brief Set the parent of a widget.
 *
 * @param widget The widget.
 * @param parent The new parent widget, or `NULL` to detach.
 */
void gui_widget_set_parent(gui_widget_t* widget, gui_widget_t* parent);

/**
 * @brief Get the ID of a widget.
 *
 * @param widget The widget.
 * @return The ID of the widget.
 */
gui_widget_id_t gui_widget_get_id(gui_widget_t* widget);

/**
 * @brief Set the ID of a widget.
 *
 * @param widget The widget.
 * @param id The new ID.
 */
void gui_widget_set_id(gui_widget_t* widget, gui_widget_id_t id);

/**
 * @brief Get the cursor for a widget.
 *
 * @param widget The widget.
 * @return The cursor type.
 */
gui_cursor_t gui_widget_get_cursor(gui_widget_t* widget);

/**
 * @brief Set the cursor for a widget.
 *
 * @param widget The widget.
 * @param cursor The new cursor type.
 */
void gui_widget_set_cursor(gui_widget_t* widget, gui_cursor_t cursor);

/**
 * @brief Get the custom user data associated with a widget.
 *
 * @param widget The widget.
 * @return The user data pointer.
 */
void* gui_widget_get_userdata(gui_widget_t* widget);

/**
 * @brief Set custom user data for a widget.
 *
 * @param widget The widget.
 * @param userdata The user data pointer.
 */
void gui_widget_set_userdata(gui_widget_t* widget, void* userdata);

/** @} */
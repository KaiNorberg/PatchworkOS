#include "internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool loaded = false;

typedef struct
{
    list_entry_t entry;
    theme_color_set_t set;
    theme_color_role_t role;
    pixel_t color;
} theme_override_color_t;

typedef struct
{
    list_entry_t entry;
    theme_string_t name;
    char* string;
} theme_override_string_t;

typedef struct
{
    list_entry_t entry;
    theme_int_t name;
    int64_t integer;
} theme_override_int_t;

typedef struct theme_override_buffer
{
    list_t colors;
    list_t strings;
    list_t integers;
} theme_override_buffer_t;

typedef struct
{
    pixel_t colors[COLOR_ROLE_AMOUNT];
} theme_colors_t;

static theme_colors_t sets[COLOR_SET_AMOUNT];

static const char* strings[STRING_AMOUNT];
static int64_t integers[INT_AMOUNT];

static void theme_colors_load(config_t* config, const char* section, theme_colors_t* dest)
{
    dest->colors[COLOR_ROLE_BACKGROUND_NORMAL] = config_int_get(config, section, "background_normal", COLOR_INVALID);
    dest->colors[COLOR_ROLE_BACKGROUND_SELECTED_START] =
        config_int_get(config, section, "background_selected_start", COLOR_INVALID);
    dest->colors[COLOR_ROLE_BACKGROUND_SELECTED_END] =
        config_int_get(config, section, "background_selected_end", COLOR_INVALID);
    dest->colors[COLOR_ROLE_BACKGROUND_UNSELECTED_START] =
        config_int_get(config, section, "background_unselected_start", COLOR_INVALID);
    dest->colors[COLOR_ROLE_BACKGROUND_UNSELECTED_END] =
        config_int_get(config, section, "background_unselected_end", COLOR_INVALID);

    dest->colors[COLOR_ROLE_FOREGROUND_NORMAL] = config_int_get(config, section, "foreground_normal", COLOR_INVALID);
    dest->colors[COLOR_ROLE_FOREGROUND_INACTIVE] =
        config_int_get(config, section, "foreground_inactive", COLOR_INVALID);
    dest->colors[COLOR_ROLE_FOREGROUND_LINK] = config_int_get(config, section, "foreground_link", COLOR_INVALID);
    dest->colors[COLOR_ROLE_FOREGROUND_SELECTED] =
        config_int_get(config, section, "foreground_selected", COLOR_INVALID);

    dest->colors[COLOR_ROLE_BEZEL] = config_int_get(config, section, "bezel", COLOR_INVALID);
    dest->colors[COLOR_ROLE_HIGHLIGHT] = config_int_get(config, section, "highlight", COLOR_INVALID);
    dest->colors[COLOR_ROLE_SHADOW] = config_int_get(config, section, "shadow", COLOR_INVALID);
}

static void theme_lazy_load(void)
{
    if (loaded)
    {
        return;
    }

    // Config files have safe failures.

    config_t* colorsConfig = config_open("theme", "colors");
    theme_colors_load(colorsConfig, "button", &sets[COLOR_SET_BUTTON]);
    theme_colors_load(colorsConfig, "view", &sets[COLOR_SET_VIEW]);
    theme_colors_load(colorsConfig, "element", &sets[COLOR_SET_ELEMENT]);
    theme_colors_load(colorsConfig, "panel", &sets[COLOR_SET_PANEL]);
    theme_colors_load(colorsConfig, "deco", &sets[COLOR_SET_DECO]);
    config_close(colorsConfig);

    config_t* varsConfig = config_open("theme", "vars");
    strings[STRING_WALLPAPER] = config_string_get(varsConfig, "strings", "wallpaper", "");
    strings[STRING_FONTS_DIR] = config_string_get(varsConfig, "strings", "fonts_dir", "");
    strings[STRING_CURSOR_ARROW] = config_string_get(varsConfig, "strings", "cursor_arrow", "");
    strings[STRING_DEFAULT_FONT] = config_string_get(varsConfig, "strings", "default_font", "");
    strings[STRING_ICON_CLOSE] = config_string_get(varsConfig, "strings", "icon_close", "");

    integers[INT_FRAME_SIZE] = config_int_get(varsConfig, "integers", "frame_size", 1);
    integers[INT_BEZEL_SIZE] = config_int_get(varsConfig, "integers", "bezel_size", 1);
    integers[INT_TITLEBAR_SIZE] = config_int_get(varsConfig, "integers", "titlebar_size", 1);
    integers[INT_PANEL_SIZE] = config_int_get(varsConfig, "integers", "panel_size", 1);
    integers[INT_BIG_PADDING] = config_int_get(varsConfig, "integers", "big_padding", 1);
    integers[INT_SMALL_PADDING] = config_int_get(varsConfig, "integers", "small_padding", 1);
    config_close(varsConfig);

    loaded = true;
}

pixel_t theme_color_get(theme_color_set_t set, theme_color_role_t role, theme_override_t* override)
{
    theme_lazy_load();

    if (set >= COLOR_SET_AMOUNT || role >= COLOR_ROLE_AMOUNT)
    {
        return COLOR_INVALID;
    }

    if (override != NULL && override->buffer != NULL)
    {
        theme_override_color_t* color;
        LIST_FOR_EACH(color, &override->buffer->colors, entry)
        {
            if (color->set == set && color->role == role)
            {
                return color->color;
            }
        }
    }

    return sets[set].colors[role];
}

const char* theme_string_get(theme_string_t name, theme_override_t* override)
{
    theme_lazy_load();

    if (name >= STRING_AMOUNT)
    {
        return "";
    }

    if (override != NULL && override->buffer != NULL)
    {
        theme_override_string_t* string;
        LIST_FOR_EACH(string, &override->buffer->strings, entry)
        {
            if (string->name == name)
            {
                return string->string;
            }
        }
    }

    return strings[name];
}

int64_t theme_int_get(theme_int_t name, theme_override_t* override)
{
    theme_lazy_load();

    if (name >= INT_AMOUNT)
    {
        return 0;
    }

    if (override != NULL && override->buffer != NULL)
    {
        theme_override_int_t* integer;
        LIST_FOR_EACH(integer, &override->buffer->integers, entry)
        {
            if (integer->name == name)
            {
                return integer->integer;
            }
        }
    }

    return integers[name];
}

void theme_override_init(theme_override_t* override)
{
    if (override == NULL)
    {
        return;
    }

    override->buffer = NULL;
}

void theme_override_deinit(theme_override_t* override)
{
    if (override == NULL || override->buffer == NULL)
    {
        return;
    }

    void* temp;

    theme_override_color_t* color;
    LIST_FOR_EACH_SAFE(color, temp, &override->buffer->colors, entry)
    {
        list_remove(&color->entry);
        free(color);
    }

    theme_override_string_t* string;
    LIST_FOR_EACH_SAFE(string, temp, &override->buffer->strings, entry)
    {
        list_remove(&string->entry);
        free(string->string);
        free(string);
    }

    theme_override_int_t* integer;
    LIST_FOR_EACH_SAFE(integer, temp, &override->buffer->integers, entry)
    {
        list_remove(&integer->entry);
        free(integer);
    }

    free(override->buffer);
    override->buffer = NULL;
}

static uint64_t theme_override_buffer_lazy_alloc(theme_override_t* override)
{
    if (override->buffer != NULL)
    {
        return 0;
    }

    override->buffer = malloc(sizeof(theme_override_buffer_t));
    if (override->buffer == NULL)
    {
        return ERR;
    }
    list_init(&override->buffer->colors);
    list_init(&override->buffer->strings);
    list_init(&override->buffer->integers);

    return 0;
}

uint64_t theme_override_color_set(theme_override_t* override, theme_color_set_t set, theme_color_role_t role,
    pixel_t color)
{
    if (override == NULL)
    {
        return ERR;
    }

    if (theme_override_buffer_lazy_alloc(override) == ERR)
    {
        return ERR;
    }

    theme_override_color_t* colorOverride;
    LIST_FOR_EACH(colorOverride, &override->buffer->colors, entry)
    {
        if (colorOverride->set == set && colorOverride->role == role)
        {
            colorOverride->color = color;
            return 0;
        }
    }

    colorOverride = (theme_override_color_t*)malloc(sizeof(theme_override_color_t));
    if (colorOverride == NULL)
    {
        return 1;
    }
    list_entry_init(&colorOverride->entry);
    colorOverride->set = set;
    colorOverride->role = role;
    colorOverride->color = color;
    list_push(&override->buffer->colors, &colorOverride->entry);
    return 0;
}

uint64_t theme_override_string_set(theme_override_t* override, theme_string_t name, const char* string)
{
    if (override == NULL || string == NULL)
    {
        return ERR;
    }

    if (theme_override_buffer_lazy_alloc(override) != 0)
    {
        return ERR;
    }

    theme_override_string_t* stringOverride;
    LIST_FOR_EACH(stringOverride, &override->buffer->strings, entry)
    {
        if (stringOverride->name == name)
        {
            char* newString = strdup(string);
            if (newString == NULL)
            {
                return ERR;
            }
            free(stringOverride->string);
            stringOverride->string = newString;
            return 0;
        }
    }

    stringOverride = malloc(sizeof(theme_override_string_t));
    if (stringOverride == NULL)
    {
        return ERR;
    }
    stringOverride->name = name;
    stringOverride->string = strdup(string);
    if (stringOverride->string == NULL)
    {
        free(stringOverride);
        return ERR;
    }
    list_push(&override->buffer->colors, &stringOverride->entry);
    return 0;
}

uint64_t theme_override_int_set(theme_override_t* override, theme_int_t name, int64_t integer)
{
    if (override == NULL)
    {
        return ERR;
    }

    if (theme_override_buffer_lazy_alloc(override) == ERR)
    {
        return ERR;
    }

    theme_override_int_t* integerOverride;
    LIST_FOR_EACH(integerOverride, &override->buffer->integers, entry)
    {
        if (integerOverride->name == name)
        {
            integerOverride->integer = integer;
            return 0;
        }
    }

    integerOverride = (theme_override_int_t*)malloc(sizeof(theme_override_int_t));
    if (integerOverride == NULL)
    {
        return ERR;
    }
    integerOverride->name = name;
    integerOverride->integer = integer;
    list_push(&override->buffer->colors, &integerOverride->entry);
    return 0;
}
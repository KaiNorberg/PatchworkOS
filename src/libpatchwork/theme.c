#include "internal.h"

#include <libpatchwork/config.h>
#include <libpatchwork/theme.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool isLoaded = false;
static theme_t theme;

static void theme_colors_load(config_t* config, const char* section, theme_color_set_t* dest)
{
    dest->backgroundNormal = config_get_int(config, section, "background_normal", THEME_COLOR_INVALID);
    dest->backgroundSelectedStart =
        config_get_int(config, section, "background_selected_start", THEME_COLOR_INVALID);
    dest->backgroundSelectedEnd =
        config_get_int(config, section, "background_selected_end", THEME_COLOR_INVALID);
    dest->backgroundUnselectedStart =
        config_get_int(config, section, "background_unselected_start", THEME_COLOR_INVALID);
    dest->backgroundUnselectedEnd =
        config_get_int(config, section, "background_unselected_end", THEME_COLOR_INVALID);

    dest->foregroundNormal = config_get_int(config, section, "foreground_normal", THEME_COLOR_INVALID);
    dest->foregroundInactive =
        config_get_int(config, section, "foreground_inactive", THEME_COLOR_INVALID);
    dest->foregroundLink = config_get_int(config, section, "foreground_link", THEME_COLOR_INVALID);
    dest->foregroundSelected =
        config_get_int(config, section, "foreground_selected", THEME_COLOR_INVALID);

    dest->bezel = config_get_int(config, section, "bezel", THEME_COLOR_INVALID);
    dest->highlight = config_get_int(config, section, "highlight", THEME_COLOR_INVALID);
    dest->shadow = config_get_int(config, section, "shadow", THEME_COLOR_INVALID);
}

static void theme_lazy_load(void)
{
    if (isLoaded)
    {
        return;
    }

    // Config files have safe failures.

    config_t* colorsConfig = config_open("theme", "colors");
    theme_colors_load(colorsConfig, "button", &theme.button);
    theme_colors_load(colorsConfig, "view", &theme.view);
    theme_colors_load(colorsConfig, "element", &theme.element);
    theme_colors_load(colorsConfig, "panel", &theme.panel);
    theme_colors_load(colorsConfig, "deco", &theme.deco);
    config_close(colorsConfig);

    config_t* varsConfig = config_open("theme", "vars");
    theme.wallpaper = config_get_string(varsConfig, "strings", "wallpaper", "");
    theme.fontsDir = config_get_string(varsConfig, "strings", "fonts_dir", "");
    theme.cursorArrow = config_get_string(varsConfig, "strings", "cursor_arrow", "");
    theme.defaultFont = config_get_string(varsConfig, "strings", "default_font", "");
    theme.iconClose = config_get_string(varsConfig, "strings", "icon_close", "");
    theme.iconMinimize = config_get_string(varsConfig, "strings", "icon_minimize", "");

    theme.frameSize = config_get_int(varsConfig, "integers", "frame_size", 1);
    theme.bezelSize = config_get_int(varsConfig, "integers", "bezel_size", 1);
    theme.titlebarSize = config_get_int(varsConfig, "integers", "titlebar_size", 1);
    theme.panelSize = config_get_int(varsConfig, "integers", "panel_size", 1);
    theme.bigPadding = config_get_int(varsConfig, "integers", "big_padding", 1);
    theme.smallPadding = config_get_int(varsConfig, "integers", "small_padding", 1);
    theme.separatorSize = config_get_int(varsConfig, "integers", "separator_size", 1);
    config_close(varsConfig);

    isLoaded = true;
}

theme_t* theme_global_get(void)
{
    theme_lazy_load();
    return &theme;
}

#include "splash.h"

#include "common/version.h"
#include "hpet.h"
#include "smp.h"
#include "sys/win.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>
#include <sys/gfx.h>

static win_theme_t theme;
static psf_t font;
static surface_t surface;

static void splash_text(int64_t offset, uint8_t scale, const char* string, pixel_t color)
{
    point_t pos = (point_t){
        .x = surface.width / 2 - strlen(string) * scale * PSF_WIDTH / 2,
        .y = surface.height / 2 - offset,
    };
    point_t shadowPos = (point_t){
        .x = pos.x + SPLASH_SHADOW_OFFSET * scale,
        .y = pos.y + SPLASH_SHADOW_OFFSET * scale,
    };

    font.scale = scale;
    font.foreground = theme.background - 0x00333333;
    font.background = theme.background;
    gfx_psf_string(&surface, &font, &shadowPos, string);

    font.foreground = color;
    font.background = theme.background;
    gfx_psf_string(&surface, &font, &pos, string);
}

void splash_init(GopBuffer* gopBuffer, BootFont* screenFont)
{
    win_default_theme(&theme);

    font.scale = SPLASH_NAME_SCALE;
    font.glyphs = malloc(screenFont->glyphsSize);
    memcpy(font.glyphs, screenFont->glyphs, screenFont->glyphsSize);

    surface.buffer = gopBuffer->base;
    surface.height = gopBuffer->height;
    surface.width = gopBuffer->width;
    surface.stride = gopBuffer->stride;
    memset(surface.buffer, 0, surface.height * surface.stride * sizeof(pixel_t));

    rect_t windowRect = (rect_t){
        .left = surface.width / 2 - SPLASH_WIDTH / 2,
        .top = surface.height / 2 - SPLASH_HEIGHT / 2,
        .right = surface.width / 2 + SPLASH_WIDTH / 2,
        .bottom = surface.height / 2 + SPLASH_HEIGHT / 2,
    };
    gfx_rect(&surface, &windowRect, theme.background);
    gfx_edge(&surface, &windowRect, theme.edgeWidth, theme.highlight, theme.shadow);

    splash_text(SPLASH_NAME_OFFSET, SPLASH_NAME_SCALE, OS_NAME, 0xFF000000);
    splash_text(SPLASH_VERSION_OFFSET, SPLASH_VERSION_SCALE, OS_VERSION, 0xFF000000);

    SPLASH_FUNC();
}

void splash_print(const char* string, pixel_t color)
{
    rect_t rect = (rect_t){
        .left = surface.width / 2 - SPLASH_WIDTH / 2 + 32,
        .top = surface.height / 2 - SPLASH_MESSAGE_OFFSET - 10,
        .right = surface.width / 2 + SPLASH_WIDTH / 2 - 32,
        .bottom = surface.height / 2 - SPLASH_MESSAGE_OFFSET + PSF_HEIGHT * SPLASH_MESSAGE_SCALE + 10,
    };
    gfx_rect(&surface, &rect, theme.background);
    gfx_edge(&surface, &rect, theme.edgeWidth, theme.shadow, theme.highlight);

    splash_text(SPLASH_MESSAGE_OFFSET, SPLASH_MESSAGE_SCALE, string, color);
}

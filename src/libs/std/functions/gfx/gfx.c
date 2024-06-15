#include <stdint.h>
#include <sys/gfx.h>

void gfx_rect(surface_t* surface, const rect_t* rect, pixel_t pixel)
{
    for (uint64_t x = rect->left; x < rect->right; x++)
    {
        for (uint64_t y = rect->top; y < rect->bottom; y++)
        {
            surface->buffer[x + y * surface->stride] = pixel;
        }
    }
}

void gfx_edge(surface_t* surface, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    rect_t leftRect = (rect_t){
        .left = rect->left,
        .top = rect->top,
        .right = rect->left + width,
        .bottom = rect->bottom - width,
    };
    gfx_rect(surface, &leftRect, foreground);

    rect_t topRect = (rect_t){
        .left = rect->left + width,
        .top = rect->top,
        .right = rect->right - width,
        .bottom = rect->top + width,
    };
    gfx_rect(surface, &topRect, foreground);

    rect_t rightRect = (rect_t){
        .left = rect->right - width,
        .top = rect->top + width,
        .right = rect->right,
        .bottom = rect->bottom,
    };
    gfx_rect(surface, &rightRect, background);

    rect_t bottomRect = (rect_t){
        .left = rect->left + width,
        .top = rect->bottom - width,
        .right = rect->right - width,
        .bottom = rect->bottom,
    };
    gfx_rect(surface, &bottomRect, background);

    for (uint64_t y = 0; y < width; y++)
    {
        for (uint64_t x = 0; x < width; x++)
        {
            pixel_t color = x + y < width ? foreground : background;
            surface->buffer[(rect->right - width + x) + (rect->top + y) * surface->stride] = color;
            surface->buffer[(rect->left + x) + (rect->bottom - width + y) * surface->stride] = color;
        }
    }
}

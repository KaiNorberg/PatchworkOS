#include <stdint.h>
#include <sys/gfx.h>

void gfx_rect(pixel_t* buffer, uint64_t width, const rect_t* rect, pixel_t pixel)
{
    for (uint64_t x = rect->left; x < rect->right; x++)
    {
        for (uint64_t y = rect->top; y < rect->bottom; y++)
        {
            buffer[x + y * width] = pixel;
        }
    }
}

void gfx_edge(pixel_t* buffer, uint64_t width, const rect_t* rect, uint64_t edgeWidth, pixel_t foreground, pixel_t background)
{
    rect_t leftRect = (rect_t){
        .left = rect->left,
        .top = rect->top,
        .right = rect->left + edgeWidth,
        .bottom = rect->bottom - edgeWidth,
    };
    gfx_rect(buffer, width, &leftRect, foreground);

    rect_t topRect = (rect_t){
        .left = rect->left + edgeWidth,
        .top = rect->top,
        .right = rect->right - edgeWidth,
        .bottom = rect->top + edgeWidth,
    };
    gfx_rect(buffer, width, &topRect, foreground);

    rect_t rightRect = (rect_t){
        .left = rect->right - edgeWidth,
        .top = rect->top + edgeWidth,
        .right = rect->right,
        .bottom = rect->bottom,
    };
    gfx_rect(buffer, width, &rightRect, background);

    rect_t bottomRect = (rect_t){
        .left = rect->left + edgeWidth,
        .top = rect->bottom - edgeWidth,
        .right = rect->right - edgeWidth,
        .bottom = rect->bottom,
    };
    gfx_rect(buffer, width, &bottomRect, background);

    for (uint64_t y = 0; y < edgeWidth; y++)
    {
        for (uint64_t x = 0; x < edgeWidth; x++)
        {
            pixel_t color = x + y < edgeWidth ? foreground : background;
            buffer[(rect->right - edgeWidth + x) + (rect->top + y) * width] = color;
            buffer[(rect->left + x) + (rect->bottom - edgeWidth + y) * width] = color;
        }
    }
}

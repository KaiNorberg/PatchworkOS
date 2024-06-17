#include <stdint.h>
#include <sys/gfx.h>

void gfx_rect(surface_t* surface, const rect_t* rect, pixel_t pixel)
{
    uint64_t pixel64 = ((uint64_t)pixel << 32) | pixel;

    for (uint64_t y = rect->top; y < rect->bottom; y++)
    {
        uint64_t count = (rect->right - rect->left) * sizeof(pixel_t);
        uint8_t* ptr = (uint8_t*)&surface->buffer[rect->left + y * surface->stride];

        while (count >= 64)
        {
            *(uint64_t*)(ptr + 0) = pixel64;
            *(uint64_t*)(ptr + 8) = pixel64;
            *(uint64_t*)(ptr + 16) = pixel64;
            *(uint64_t*)(ptr + 24) = pixel64;
            *(uint64_t*)(ptr + 32) = pixel64;
            *(uint64_t*)(ptr + 40) = pixel64;
            *(uint64_t*)(ptr + 48) = pixel64;
            *(uint64_t*)(ptr + 56) = pixel64;
            ptr += 64;
            count -= 64;
        }

        while (count >= 8)
        {
            *(uint64_t*)ptr = pixel64;
            ptr += 8;
            count -= 8;
        }

        while (count--)
        {
            *ptr++ = pixel;
        }
    }
}

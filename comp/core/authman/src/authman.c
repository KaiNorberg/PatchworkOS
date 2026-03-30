#include <ft2build.h>
#include <libc/io.h>
#include <libc/math.h>
#include <libdraw/draw.h>
#include <libwidget/widget.h>
#include <stdio.h>
#include <time.h>
#include FT_FREETYPE_H

int main(int argc, char** argv)
{
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    char format[MAX_NAME];
    while (true)
    {
        status_t status =
            ioscanp(FDCWD, FDROOT, "/dev/fb/0/info", MAX_PATH, 0, NULL, "%u %u %u %s", &width, &height, &pitch, format);
        if (!IS_ERR(status))
        {
            break;
        }

        if (!IS_CODE(status, NOENT))
        {
            printf("authman: failed to get framebuffer info %Y\n", status);
            return EXIT_FAILURE;
        }
    }

    void* address;
    status_t status = iomapp(FDCWD, FDROOT, "/dev/fb/0/data", &address, height * pitch, 0, IOMAP_READ | IOMAP_WRITE);
    if (IS_ERR(status))
    {
        printf("authman: failed to map framebuffer %Y\n", status);
        return EXIT_FAILURE;
    }

    printf("authman: framebuffer %ux%u pitch %u format %s\n", width, height, pitch, format);

    if (strcmp(format, "B8G8R8A8") != 0)
    {
        printf("authman: unsupported framebuffer format %s\n", format);
        return EXIT_FAILURE;
    }

    drawable_t screen;
    draw_init(&screen, address, width, height, pitch);

    pixel_t* backbuffer = malloc(height * pitch);
    if (backbuffer == NULL)
    {
        printf("authman: failed to allocate backbuffer\n");
        return EXIT_FAILURE;
    }

    drawable_t back;
    draw_init(&back, backbuffer, width, height, pitch);

    pixel_t* frontbuffer = malloc(height * pitch);
    if (frontbuffer == NULL)
    {
        printf("authman: failed to allocate frontbuffer\n");
        return EXIT_FAILURE;
    }

    drawable_t front;
    draw_init(&front, frontbuffer, width, height, pitch);

    const size_t vertexAmount = 360;
    float vertices[vertexAmount * 2];
    const size_t x = sizeof(vertices);
    printf("authman: vertices size %zu\n", x);

    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        printf("authman: failed to initialize freetype\n");
    }
    else
    {
        printf("authman: freetype initialized successfully\n");
        FT_Done_FreeType(ft);
    }

    vertices_circle(vertices, vertexAmount, width / 2, height / 2, MIN(width, height) / 2, 0, 2.0 * M_PI);

    polygon_t* circle = polygon_new(vertices, vertexAmount);
    if (circle == NULL)
    {
        printf("authman: failed to create circle\n");
        return EXIT_FAILURE;
    }

    clock_t check = clock();
    size_t iterations = 0;
    while (true)
    {
        draw_fill(&front, PIXEL_ARGB(255, 255, 0, 0));
        draw_fill(&back, PIXEL_ARGB(0, 0, 0, 0));

        draw_polygon(&back, circle, PIXEL_ARGB(255, 255, 255, 255), DRAW_BLEND_SET);

        draw_blit(&front, &back, RECT(0, 0, width, height), RECT(0, 0, width, height));

        draw_transfer(&screen, &front);

        iterations++;
        if (clock() - check > CLOCKS_PER_SEC)
        {
            printf("authman: %zu fps\n", iterations);
            iterations = 0;
            check = clock();
        }
    }

    return 0;
}

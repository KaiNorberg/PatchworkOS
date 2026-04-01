#include <libc/io.h>
#include <libc/math.h>
#include <libgfx/gfx.h>
#include <libgui/gui.h>
#include <stdio.h>
#include <time.h>

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

    gfx_pixel_t background = GFX_PIXEL(255, 56, 56, 56);
    memset32(address, background.argb, height * pitch / sizeof(gfx_pixel_t));

    gfx_t screen = GFX(address, width, height, pitch);

    gui_t* gui;
    status = gui_new(&screen, &gui);
    if (IS_ERR(status))
    {
        printf("authman: failed to create gui %Y\n", status);
        return EXIT_FAILURE;
    }

    gui_widget_t* testButton;
    status = gui_button_new(gui_get_root(gui), 0, GFX_RECT_FROM_CENTER(width / 2, height / 2, 100, 100), &testButton);
    if (IS_ERR(status))
    {
        printf("authman: failed to create button %Y\n", status);
        return EXIT_FAILURE;
    }

    gui_widget_show(testButton);

    clock_t now = clock();
    clock_t last = now;
    while (1)
    {
        clock_t next = gui_next_timeout(gui);

        clock_t timeToSleep = MIN(next, CLOCKS_PER_SEC / 60);
        if (timeToSleep > 0)
        {
            struct timespec ts = {
                .tv_sec = timeToSleep / CLOCKS_PER_SEC,
                .tv_nsec = timeToSleep % CLOCKS_PER_SEC,
            };

            thrd_sleep(&ts, NULL);
        }

        now = clock();
        clock_t delta = now - last;
        last = now;

        printf("authman: tick\n");
        gui_tick(gui, delta);
    }

    /*gfx_t screen = GFX(address, width, height, pitch);

    gfx_pixel_t* backbuffer = malloc(height * pitch);
    if (backbuffer == NULL)
    {
        printf("authman: failed to allocate backbuffer\n");
        return EXIT_FAILURE;
    }

    gfx_t back = GFX(backbuffer, width, height, pitch);

    gfx_pixel_t* frontbuffer = malloc(height * pitch);
    if (frontbuffer == NULL)
    {
        printf("authman: failed to allocate frontbuffer\n");
        return EXIT_FAILURE;
    }

    gfx_t front = GFX(frontbuffer, width, height, pitch);

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

    gfx_verts_circle(vertices, vertexAmount, width / 2, height / 2, MIN(width, height) / 2, 0, 2.0 * M_PI);

    gfx_poly_t* circle = gfx_poly_new(vertices, vertexAmount);
    if (circle == NULL)
    {
        printf("authman: failed to create circle\n");
        return EXIT_FAILURE;
    }

    clock_t check = clock();
    size_t iterations = 0;
    while (true)
    {
        gfx_draw_fill(&front, GFX_PIXEL(255, 255, 0, 0));
        gfx_draw_fill(&back, GFX_PIXEL(0, 0, 0, 0));

        gfx_draw_polygon(&back, circle, GFX_PIXEL(255, 255, 255, 255), GFX_BLEND_SET);

        gfx_draw_blit(&front, &back, GFX_RECT(0, 0, width, height), GFX_RECT(0, 0, width, height));

        gfx_draw_transfer(&screen, &front);

        iterations++;
        if (clock() - check > CLOCKS_PER_SEC)
        {
            printf("authman: %zu fps\n", iterations);
            iterations = 0;
            check = clock();
        }
    }*/

    return 0;
}

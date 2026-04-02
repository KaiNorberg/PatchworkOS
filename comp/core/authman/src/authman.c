#include <libc/io.h>
#include <libc/math.h>
#include <libgfx/gfx.h>
#include <libgui/cursor.h>
#include <libgui/gui.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char** argv)
{
    // This is just testing code for now.

    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    char format[MAX_NAME];
    status_t status = RETRY(
        ioscanp(FDCWD, FDROOT, "/dev/fb/0/info", MAX_PATH, 0, NULL, "%u %u %u %s", &width, &height, &pitch, format));
    if (IS_ERR(status))
    {
        printf("authman: failed to get framebuffer info %Y\n", status);
        return EXIT_FAILURE;
    }

    void* address = NULL;
    status = iomapp(FDCWD, FDROOT, "/dev/fb/0/data", &address, height * pitch, 0, IOMAP_READ | IOMAP_WRITE);
    if (IS_ERR(status))
    {
        printf("authman: failed to map framebuffer %Y\n", status);
        return EXIT_FAILURE;
    }

    fd_t kbd;
    status = RETRY(iowalk(FDCWD, FDROOT, "/dev/kbd/0/events", &kbd));
    if (IS_ERR(status))
    {
        printf("authman: failed to open keyboard %Y\n", status);
        return EXIT_FAILURE;
    }

    fd_t mouse;
    status = RETRY(iowalk(FDCWD, FDROOT, "/dev/mouse/0/events", &mouse));
    if (IS_ERR(status))
    {
        printf("authman: failed to open mouse %Y\n", status);
        return EXIT_FAILURE;
    }

    printf("authman: framebuffer %ux%u pitch %u format %s\n", width, height, pitch, format);

    if (strcmp(format, "B8G8R8A8") != 0)
    {
        printf("authman: unsupported framebuffer format %s\n", format);
        return EXIT_FAILURE;
    }

    gfx_t screen = GFX(address, width, height, pitch);

    gfx_pixel_t* backbuffer = malloc(height * pitch);
    if (backbuffer == NULL)
    {
        printf("authman: failed to allocate backbuffer\n");
        return EXIT_FAILURE;
    }

    gfx_t back = GFX(backbuffer, width, height, pitch);

    gui_t* gui;
    status = gui_new(&back, &gui);
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

    gui_cursor_theme_t* theme;
    status = gui_cursor_theme_load(&theme);
    if (IS_ERR(status))
    {
        printf("authman: failed to load cursor theme %Y\n", status);
        return EXIT_FAILURE;
    }

    gui_cursor_state_t* state;
    status = gui_cursor_state_new(theme, 32, 32, &state);
    if (IS_ERR(status))
    {
        printf("authman: failed to create cursor state %Y\n", status);
        return EXIT_FAILURE;
    }

    gfx_pixel_t background = GUI_THEME_WINDOW_BACKGROUND;
    memset32(address, background.argb, height * pitch / sizeof(gfx_pixel_t));
    memset32(backbuffer, background.argb, height * pitch / sizeof(gfx_pixel_t));

    gui_widget_show(testButton);

    int32_t mouseX = 100;
    int32_t mouseY = 100;

    clock_t now = clock();
    clock_t last = now;
    gui_cursor_state_update(state, GUI_CURSOR_LEFT_PTR_WATCH, now);

    while (1)
    {
        clock_t timeout = MIN(gui_next_timeout(gui, now), gui_cursor_state_next_frame(state, now));

        char buffer[256];
        size_t bytesRead;
        status = ioreadt(mouse, IOBUF(buffer, sizeof(buffer)), 0, timeout, &bytesRead);
        if (IS_ERR(status) && !IS_CODE(status, TIMEOUT))
        {
            printf("authman: failed to read mouse %Y\n", status);
            return EXIT_FAILURE;
        }

        if (bytesRead > 0 && !IS_CODE(status, TIMEOUT))
        {
            const char* p = buffer;
            while (p + 4 < buffer + bytesRead)
            {
                char sign = p[0];
                uint8_t value = (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
                char type = p[4];
                p += 5;

                switch (type)
                {
                case 'x':
                    mouseX += value * (sign == '-' ? -1 : 1);
                    mouseX = CLAMP(mouseX, 0, (int32_t)width);
                    gui_input_mouse(gui, mouseX, mouseY, 0, GUI_MOUSE_BUTTON_NONE, GUI_MOUSE_BUTTON_NONE);
                    break;
                case 'y':
                    mouseY += value * (sign == '-' ? -1 : 1);
                    mouseY = CLAMP(mouseY, 0, (int32_t)height);
                    gui_input_mouse(gui, mouseX, mouseY, 0, GUI_MOUSE_BUTTON_NONE, GUI_MOUSE_BUTTON_NONE);
                    break;
                case 'z':
                    gui_input_mouse(gui, mouseX, mouseY, value, GUI_MOUSE_BUTTON_NONE, GUI_MOUSE_BUTTON_NONE);
                    break;
                case '^':
                    gui_input_mouse(gui, mouseX, mouseY, 0, GUI_MOUSE_BUTTON_NONE, (1 << value));
                    break;
                case '_':                    
                    gui_input_mouse(gui, mouseX, mouseY, 0, (1 << value), GUI_MOUSE_BUTTON_NONE);
                    break;
                default:
                    break;
                }
            }
        }

        now = clock();
        clock_t delta = now - last;
        last = now;

        gui_cursor_state_update(state, gui_widget_get_cursor(gui_get_hovered(gui)), now);

        gfx_region_t dirty;
        gui_get_dirty(gui, &dirty);

        gfx_rect_t oldCursorBounds = gui_cursor_state_get_bounds(state);
        gui_cursor_state_clear(state, &back);

        gui_tick(gui, delta);

        gui_cursor_state_draw(state, &back, mouseX, mouseY);

        gfx_draw_copy(&screen, &back, oldCursorBounds, oldCursorBounds);
        for (size_t i = 0; i < dirty.count; i++)
        {
            gfx_draw_copy(&screen, &back, dirty.rects[i], dirty.rects[i]);
        }

        gfx_rect_t cursorBounds = gui_cursor_state_get_bounds(state);
        gfx_draw_copy(&screen, &back, cursorBounds, cursorBounds);
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

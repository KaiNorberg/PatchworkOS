#include <libpatchwork/patchwork.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 500

static void draw_hand(drawable_t* draw, point_t center, double angle, uint64_t length, uint64_t width, pixel_t color)
{
    double cosAngle = cos(angle);
    double sinAngle = sin(angle);

    for (int64_t w = -((int64_t)width / 2); w <= (int64_t)(width / 2); w++)
    {
        point_t start = {
            .x = center.x + (int64_t)(-w * sinAngle),
            .y = center.y + (int64_t)(w * cosAngle),
        };
        point_t end = {
            .x = center.x + (int64_t)(length * cosAngle) - (int64_t)(w * sinAngle),
            .y = center.y + (int64_t)(length * sinAngle) + (int64_t)(w * cosAngle),
        };
        draw_line(draw, &start, &end, color, 2);
    }
}

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    (void)win; // Unused

    switch (event->type)
    {
    case LEVENT_INIT:
    {

    }
    break;
    case LEVENT_DEINIT:
    {

    }
    break;
    case LEVENT_REDRAW:
    {
        drawable_t draw;
        element_draw_begin(elem, &draw);

        time_t now = time(NULL);
        struct tm* t = localtime(&now);

        rect_t clockRect = RECT_INIT_DIM(50, 50, 400, 400);
        draw_rect(&draw, &clockRect, PIXEL_ARGB(255, 255, 255, 255)); // White background
        point_t center = {clockRect.left + RECT_WIDTH(&clockRect) / 2, clockRect.top + RECT_HEIGHT(&clockRect) / 2};
        uint64_t radius = RECT_WIDTH(&clockRect) / 2 - 20;

        for (int i = 0; i < 12; i++)
        {
            draw_hand(&draw, center, (double)i * M_PI / 6.0, radius - 10, 4, PIXEL_ARGB(255, 0, 0, 0)); // Black hour marks
        }

        element_draw_end(elem, &draw);
    }
    break;
    }
    return 0;
}

int main(void)
{
    display_t* disp = display_new();
    if (disp == NULL)
    {
        return EXIT_FAILURE;
    }

    rect_t rect = RECT_INIT_DIM(500, 200, WINDOW_WIDTH, WINDOW_HEIGHT);
    window_t* win = window_new(disp, "Clock", &rect, SURFACE_WINDOW, WINDOW_DECO, procedure, NULL);
    if (win == NULL)
    {
        display_free(disp);
        return EXIT_FAILURE;
    }

    if (window_set_visible(win, true) == ERR)
    {
        window_free(win);
        display_free(disp);
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_next(disp, &event, CLOCKS_NEVER) != ERR)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);
    return EXIT_SUCCESS;
}

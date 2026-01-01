#include <libpatchwork/patchwork.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/defs.h>
#include <time.h>

#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 500

static point_t hourMarker[] = {{-3, 0}, {3, 0}, {2, -30}, {-2, -30}};

static point_t minuteMarker[] = {{-1, 0}, {1, 0}, {1, -15}, {-1, -15}};

static point_t hourHand[] = {{-8, 15}, {8, 15}, {6, -50}, {3, -75}, {0, -85}, {-3, -75}, {-6, -50}};

static point_t minuteHand[] = {{-6, 15}, {6, 15}, {4, -120}, {2, -145}, {0, -155}, {-2, -145}, {-4, -120}};

static point_t secondHand[] = {{-2, 30}, {2, 30}, {2, 0}, {1, -165}, {0, -175}, {-1, -165}, {-2, 0}};

static void draw_marker(drawable_t* draw, point_t center, int64_t radius, const point_t* markerPoints,
    uint64_t pointCount, double angle, pixel_t pixel)
{
    point_t rotatedMarker[pointCount];
    memcpy(rotatedMarker, markerPoints, sizeof(point_t) * pointCount);

    for (uint64_t i = 0; i < 4; i++)
    {
        rotatedMarker[i].x += center.x;
        rotatedMarker[i].y += center.y - radius;
    }

    polygon_rotate(rotatedMarker, pointCount, angle, center);
    draw_polygon(draw, rotatedMarker, pointCount, pixel);
}

static void draw_hand(drawable_t* draw, point_t center, const point_t* handPoints, uint64_t pointCount, double angle,
    pixel_t pixel)
{
    point_t rotatedHand[pointCount];
    memcpy(rotatedHand, handPoints, sizeof(point_t) * pointCount);
    for (uint64_t i = 0; i < pointCount; i++)
    {
        rotatedHand[i].x += center.x;
        rotatedHand[i].y += center.y;
    }
    polygon_rotate(rotatedHand, pointCount, angle, center);
    draw_polygon(draw, rotatedHand, pointCount, pixel);
}

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    UNUSED(win);

    switch (event->type)
    {
    case EVENT_LIB_INIT:
    {
        window_set_timer(win, TIMER_REPEAT, CLOCKS_PER_SEC / 2);
    }
    break;
    case EVENT_LIB_DEINIT:
    {
    }
    break;
    case EVENT_LIB_QUIT:
    {
        display_disconnect(window_get_display(win));
    }
    break;
    case EVENT_TIMER:
    case EVENT_LIB_REDRAW:
    {
        drawable_t draw;
        element_draw_begin(elem, &draw);

        time_t now = time(NULL);
        struct tm* t = localtime(&now);

        const theme_t* theme = element_get_theme(elem);

        rect_t clockRect = element_get_content_rect(elem);
        draw_rect(&draw, &clockRect, theme->deco.backgroundNormal);
        point_t center = {clockRect.left + RECT_WIDTH(&clockRect) / 2, clockRect.top + RECT_HEIGHT(&clockRect) / 2};
        uint64_t radius = RECT_WIDTH(&clockRect) / 2 - 75;

        for (int i = 0; i < 12; i++)
        {
            double angle = i * (M_PI / 6);
            draw_marker(&draw, center, radius, hourMarker, ARRAY_SIZE(hourMarker), angle, PIXEL_ARGB(255, 0, 0, 0));
        }

        for (int i = 0; i < 60; i++)
        {
            if (i % 5 != 0)
            {
                double angle = i * (M_PI / 30);
                draw_marker(&draw, center, radius, minuteMarker, ARRAY_SIZE(minuteMarker), angle,
                    PIXEL_ARGB(255, 100, 100, 100));
            }
        }

        double hourAngle = ((t->tm_hour % 12) + (double)t->tm_min / 60.0) * (M_PI / 6);
        double minuteAngle = (t->tm_min + (double)t->tm_sec / 60.0) * (M_PI / 30);
        double secondAngle = t->tm_sec * (M_PI / 30);

        draw_hand(&draw, center, hourHand, ARRAY_SIZE(hourHand), hourAngle, PIXEL_ARGB(255, 0, 0, 0));
        draw_hand(&draw, center, minuteHand, ARRAY_SIZE(minuteHand), minuteAngle, PIXEL_ARGB(255, 0, 0, 0));
        draw_hand(&draw, center, secondHand, ARRAY_SIZE(secondHand), secondAngle, PIXEL_ARGB(255, 255, 0, 0));
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

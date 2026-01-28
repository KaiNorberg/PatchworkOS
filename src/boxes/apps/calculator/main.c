#include <patchwork/patchwork.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LABEL_ID 1234
#define LABEL_HEIGHT (42)

#define NUMPAD_COLUMNS 4
#define NUMPAD_ROWS 4
#define NUMPAD_PADDING 6
#define NUMPAD_BUTTON_WIDTH (64)

#define NUMPAD_COLUMN_TO_WINDOW(column) (NUMPAD_PADDING * ((column) + 1) + NUMPAD_BUTTON_WIDTH * (column))
#define NUMPAD_ROW_TO_WINDOW(row) (LABEL_HEIGHT + NUMPAD_PADDING * ((row) + 2) + NUMPAD_BUTTON_WIDTH * (row))

#define LABEL_WIDTH (NUMPAD_COLUMN_TO_WINDOW(NUMPAD_COLUMNS) - NUMPAD_PADDING * 2)

#define WINDOW_WIDTH (NUMPAD_COLUMN_TO_WINDOW(NUMPAD_COLUMNS))
#define WINDOW_HEIGHT (NUMPAD_ROW_TO_WINDOW(NUMPAD_ROWS))

static uint64_t numpad_button_create(element_t* elem, font_t* font, uint64_t column, uint64_t row, const char* name,
    element_id_t id)
{
    rect_t rect = RECT_INIT_DIM(NUMPAD_COLUMN_TO_WINDOW(column), NUMPAD_ROW_TO_WINDOW(row), NUMPAD_BUTTON_WIDTH,
        NUMPAD_BUTTON_WIDTH);
    element_t* button = button_new(elem, id, &rect, name, ELEMENT_NONE);
    if (button == NULL)
    {
        return PFAIL;
    }
    element_get_text_props(button)->font = font;

    return 0;
}

typedef struct
{
    uint64_t input;
    uint64_t accumulator;
    char operation;
    font_t* largeFont;
} calculator_t;

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case EVENT_LIB_INIT:
    {
        calculator_t* calc = malloc(sizeof(calculator_t));
        if (calc == NULL)
        {
            return PFAIL;
        }
        calc->input = 0;
        calc->accumulator = 0;
        calc->operation = '=';
        calc->largeFont = font_new(window_get_display(win), "default", "regular", 32);
        if (calc->largeFont == NULL)
        {
            free(calc);
            return PFAIL;
        }

        for (uint64_t column = 0; column < 3; column++)
        {
            for (uint64_t row = 0; row < 3; row++)
            {
                element_id_t id = 9 - ((2 - column) + row * 3);
                char name[2] = {'0' + id, '\0'};

                if (numpad_button_create(elem, calc->largeFont, column, row, name, id) == PFAIL)
                {
                    font_free(calc->largeFont);
                    free(calc);
                    return PFAIL;
                }
            }
        }

        if (numpad_button_create(elem, calc->largeFont, 1, 3, "0", 0) == PFAIL ||
            numpad_button_create(elem, calc->largeFont, 3, 0, "/", '/') == PFAIL ||
            numpad_button_create(elem, calc->largeFont, 3, 1, "*", '*') == PFAIL ||
            numpad_button_create(elem, calc->largeFont, 3, 2, "-", '-') == PFAIL ||
            numpad_button_create(elem, calc->largeFont, 3, 3, "+", '+') == PFAIL ||
            numpad_button_create(elem, calc->largeFont, 0, 3, "<", '<') == PFAIL ||
            numpad_button_create(elem, calc->largeFont, 2, 3, "=", '=') == PFAIL)
        {
            font_free(calc->largeFont);
            free(calc);
            return PFAIL;
        }

        rect_t labelRect = RECT_INIT_DIM(NUMPAD_PADDING, NUMPAD_PADDING, LABEL_WIDTH, LABEL_HEIGHT);
        element_t* label = label_new(elem, LABEL_ID, &labelRect, "0", ELEMENT_NONE);
        if (label == NULL)
        {
            font_free(calc->largeFont);
            free(calc);
            return PFAIL;
        }

        text_props_t* props = element_get_text_props(label);
        props->font = calc->largeFont;
        props->xAlign = ALIGN_MAX;

        element_set_private(elem, calc);
    }
    break;
    case EVENT_LIB_DEINIT:
    {
        calculator_t* calc = element_get_private(elem);
        if (calc == NULL)
        {
            break;
        }
        font_free(calc->largeFont);
        free(calc);
    }
    break;
    case EVENT_LIB_ACTION:
    {
        if (event->libAction.type != ACTION_RELEASE)
        {
            break;
        }

        calculator_t* calc = element_get_private(elem);

        element_t* label = element_find(elem, LABEL_ID);
        if (label == NULL)
        {
            return PFAIL;
        }

        if (event->libAction.source <= 9)
        {
            calc->input = calc->input * 10 + event->libAction.source;
        }
        else if (event->libAction.source == '<')
        {
            calc->input /= 10;
        }
        else
        {
            switch (calc->operation)
            {
            case '/':
                if (calc->input == 0)
                {
                    element_set_text(label, "DIV BY ZERO");
                    element_redraw(label, false);
                    return 0;
                }
                calc->accumulator /= calc->input;
                break;
            case '*':
                calc->accumulator *= calc->input;
                break;
            case '-':
                calc->accumulator -= calc->input;
                break;
            case '+':
                calc->accumulator += calc->input;
                break;
            case '=':
                calc->accumulator = calc->input;
                break;
            default:
                return 0;
            }
            calc->input = 0;

            calc->operation = event->libAction.source;
        }

        char buffer[32];
        ulltoa(event->libAction.source == '=' ? calc->accumulator : calc->input, buffer, 10);
        element_set_text(label, buffer);
        element_redraw(label, false);
    }
    break;
    case EVENT_LIB_QUIT:
    {
        display_disconnect(window_get_display(win));
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
    window_t* win = window_new(disp, "Calculator", &rect, SURFACE_WINDOW, WINDOW_DECO, procedure, NULL);
    if (win == NULL)
    {
        display_free(disp);
        return EXIT_FAILURE;
    }

    if (window_set_visible(win, true) == PFAIL)
    {
        window_free(win);
        display_free(disp);
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_next(disp, &event, CLOCKS_NEVER) != PFAIL)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);
    return EXIT_SUCCESS;
}

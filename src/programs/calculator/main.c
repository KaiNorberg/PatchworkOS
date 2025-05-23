#include <libdwm/dwm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

static font_t* largeFont;
static label_t* label;

static void numpad_button_create(window_t* win, element_t* elem, uint64_t column, uint64_t row, const char* label,
    element_id_t id)
{
    rect_t rect = RECT_INIT_DIM(NUMPAD_COLUMN_TO_WINDOW(column), NUMPAD_ROW_TO_WINDOW(row), NUMPAD_BUTTON_WIDTH,
        NUMPAD_BUTTON_WIDTH);
    button_new(elem, id, &rect, largeFont, windowTheme.dark, windowTheme.background, BUTTON_NONE, label);
}

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    static uint64_t input;
    static uint64_t accumulator;
    static char operation;

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        input = 0;
        accumulator = 0;
        operation = '=';

        for (uint64_t column = 0; column < 3; column++)
        {
            for (uint64_t row = 0; row < 3; row++)
            {
                element_id_t id = 9 - ((2 - column) + row * 3);
                char name[2] = {'0' + id, '\0'};

                numpad_button_create(win, elem, column, row, name, id);
            }
        }
        numpad_button_create(win, elem, 1, 3, "0", 0);
        numpad_button_create(win, elem, 3, 0, "/", '/');
        numpad_button_create(win, elem, 3, 1, "*", '*');
        numpad_button_create(win, elem, 3, 2, "-", '-');
        numpad_button_create(win, elem, 3, 3, "+", '+');
        numpad_button_create(win, elem, 0, 3, "<", '<');
        numpad_button_create(win, elem, 2, 3, "=", '=');

        rect_t labelRect = RECT_INIT_DIM(NUMPAD_PADDING, NUMPAD_PADDING, LABEL_WIDTH, LABEL_HEIGHT);
        label = label_new(elem, LABEL_ID, &labelRect, largeFont, ALIGN_MAX, ALIGN_CENTER, windowTheme.dark,
            windowTheme.bright, LABEL_NONE, "0");
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.type != ACTION_RELEASE)
        {
            break;
        }

        if (event->lAction.source <= 9)
        {
            input = input * 10 + event->lAction.source;
        }
        else if (event->lAction.source == '<')
        {
            input /= 10;
        }
        else
        {
            switch (operation)
            {
            case '/':
                if (input == 0)
                {
                    label_set_text(label, "DIV BY ZERO");
                    return 0;
                }
                accumulator /= input;
                break;
            case '*':
                accumulator *= input;
                break;
            case '-':
                accumulator -= input;
                break;
            case '+':
                accumulator += input;
                break;
            case '=':
                accumulator = input;
                break;
            default:
                return 0;
            }
            input = 0;

            operation = event->lAction.source;
        }

        char buffer[32];
        ulltoa(event->lAction.source == '=' ? accumulator : input, buffer, 10);
        label_set_text(label, buffer);
    }
    break;
    case LEVENT_QUIT:
    {
        display_disconnect(window_display(win));
    }
    break;
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_new();

    largeFont = font_new(disp, DEFAULT_FONT, 32);

    rect_t rect = RECT_INIT_DIM(500, 200, WINDOW_WIDTH, WINDOW_HEIGHT);
    window_t* win = window_new(disp, "Calculator", &rect, SURFACE_WINDOW, WINDOW_DECO, procedure, NULL);
    if (win == NULL)
    {
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_connected(disp))
    {
        display_next_event(disp, &event, CLOCKS_NEVER);
        display_dispatch(disp, &event);
    }

    window_free(win);
    font_free(largeFont);
    display_free(disp);
    return EXIT_SUCCESS;
}

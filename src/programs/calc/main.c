#include <stdint.h>
#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/kbd.h>
#include <sys/math.h>
#include <sys/proc.h>
#include <sys/win.h>

#define WINDOW_WIDTH 280
#define WINDOW_HEIGHT 330

#define NUMPAD_COLUMNS 4
#define NUMPAD_ROWS 4
#define NUMPAD_PADDING 6
#define NUMPAD_BUTTON_WIDTH ((WINDOW_WIDTH - NUMPAD_PADDING * (NUMPAD_ROWS + 1)) / NUMPAD_ROWS)
#define NUMPAD_WIDTH (NUMPAD_PADDING * (NUMPAD_ROWS + 1) + NUMPAD_BUTTON_WIDTH * NUMPAD_ROWS)

#define NUMPAD_COLUMN_TO_WINDOW(column) (NUMPAD_PADDING * ((column) + 1) + NUMPAD_BUTTON_WIDTH * (column))
#define NUMPAD_ROW_TO_WINDOW(row) (WINDOW_HEIGHT - NUMPAD_WIDTH + NUMPAD_PADDING * (row + 1) + NUMPAD_BUTTON_WIDTH * row)

#define LABEL_ID 1234

static void numpad_button_create(win_t* window, uint64_t column, uint64_t row, const char* name, widget_id_t id)
{
    rect_t rect =
        RECT_INIT_DIM(NUMPAD_COLUMN_TO_WINDOW(column), NUMPAD_ROW_TO_WINDOW(row), NUMPAD_BUTTON_WIDTH, NUMPAD_BUTTON_WIDTH);
    win_text_prop_t props = {.height = 32, .foreground = 0xFF000000, .xAlign = GFX_CENTER, .yAlign = GFX_CENTER};
    widget_t* button = win_button_new(window, name, &rect, id, &props, WIN_BUTTON_NONE);
}

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    static uint64_t input;
    static uint64_t accumulator;
    static char operation;

    switch (msg->type)
    {
    case LMSG_INIT:
    {
        input = 0;
        accumulator = 0;
        operation = '=';

        for (uint64_t column = 0; column < 3; column++)
        {
            for (uint64_t row = 0; row < 3; row++)
            {
                widget_id_t id = 9 - ((2 - column) + row * 3);
                char name[2] = {'0' + id, '\0'};

                numpad_button_create(window, column, row, name, id);
            }
        }
        numpad_button_create(window, 1, 3, "0", 0);
        numpad_button_create(window, 3, 0, "/", '/');
        numpad_button_create(window, 3, 1, "*", '*');
        numpad_button_create(window, 3, 2, "-", '-');
        numpad_button_create(window, 3, 3, "+", '+');
        numpad_button_create(window, 0, 3, "<", '<');
        numpad_button_create(window, 2, 3, "=", '=');

        rect_t labelRect = RECT_INIT_DIM(NUMPAD_PADDING, NUMPAD_PADDING, WINDOW_WIDTH - NUMPAD_PADDING * 2,
            WINDOW_HEIGHT - NUMPAD_WIDTH - NUMPAD_PADDING * 2);

        wmsg_text_prop_t props = {.height = 32, .foreground = winTheme.dark, .xAlign = GFX_MAX, .yAlign = GFX_CENTER};
        win_label_new(window, "0", &labelRect, LABEL_ID, &props);
    }
    break;
    case LMSG_COMMAND:
    {
        lmsg_command_t* data = (lmsg_command_t*)msg->data;
        if (data->type == LMSG_COMMAND_RELEASE)
        {
            if (data->id <= 9)
            {
                input = input * 10 + data->id;
            }
            else if (data->id == '<')
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
                        win_widget_name_set(win_widget(window, LABEL_ID), "DIV BY ZERO");
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
                }
                input = 0;

                operation = data->id;
            }

            char buffer[32];
            ulltoa(data->id == '=' ? accumulator : input, buffer, 10);
            win_widget_name_set(win_widget(window, LABEL_ID), buffer);
        }
    }
    break;
    }

    return 0;
}

int main(void)
{
    rect_t rect = RECT_INIT_DIM(500, 200, WINDOW_WIDTH, WINDOW_HEIGHT);
    win_expand_to_window(&rect, WIN_DECO);

    win_t* window = win_new("Calculator", &rect, DWM_WINDOW, WIN_DECO, procedure);
    if (window == NULL)
    {
        return EXIT_FAILURE;
    }

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    return EXIT_SUCCESS;
}

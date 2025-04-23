#include "input.h"

#include <string.h>

void input_init(input_t* input)
{
    input->length = 0;
    input->index = 0;
}

void input_deinit(input_t* input)
{
}

void input_insert(input_t* input, char chr)
{
    if (input->length + 1 >= MAX_PATH)
    {
        return;
    }

    if (input->index < input->length)
    {
        memmove(input->buffer + input->index + 1, input->buffer + input->index, input->length - input->index);
    }

    input->buffer[input->index] = chr;
    input->index++;
    input->length++;
    input->buffer[input->length] = '\0';
}

void input_set(input_t* input, const char* str)
{
    uint64_t len = strlen(str);
    strcpy(input->buffer, str);
    input->length = len;
    input->index = len;
}

void input_backspace(input_t* input)
{
    if (input->index == 0)
    {
        return;
    }

    input->index--;
    memmove(input->buffer + input->index, input->buffer + input->index + 1, input->length - input->index);
    input->length--;
}

uint64_t input_move(input_t* input, int64_t offset)
{
    if (offset < 0)
    {
        uint64_t absoluteOffest = -offset;
        if (absoluteOffest > input->index)
        {
            return ERR;
        }
        input->index -= absoluteOffest;
    }
    else
    {
        if (input->index + offset > input->length)
        {
            return ERR;
        }
        input->index += offset;
    }

    return 0;
}

void input_save(input_t* input)
{
    strcpy(input->savedBuffer, input->buffer);
}

void input_restore(input_t* input)
{
    input_set(input, input->savedBuffer);
}

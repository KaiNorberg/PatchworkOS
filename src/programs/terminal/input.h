#pragma once

#include <sys/io.h>

typedef struct
{
    char buffer[MAX_PATH];
    uint64_t length;
    uint64_t index;
} input_t;

void input_init(input_t* input);

void input_deinit(input_t* input);

void input_insert(input_t* input, char chr);

void input_set(input_t* input, const char* str);

void input_backspace(input_t* input);

uint64_t input_move(input_t* input, int64_t offset);

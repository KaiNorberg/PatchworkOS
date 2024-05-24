#pragma once

#include "token.h"

typedef struct
{
    const char* name;
    void (*callback)(Token*);
} Command;

void parser_parse(const char* string);
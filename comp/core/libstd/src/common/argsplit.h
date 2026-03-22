#pragma once

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const char* current;
    uint8_t escaped;
    bool inQuote;
    bool isNewArg;
    bool isFirst;
    uint64_t processedChars;
    uint64_t maxLen;
} _argsplit_state_t;

#define _ARGSPLIT_CREATE(str, maxLen) {str, 0, false, false, true, 0, maxLen}

bool _argsplit_step_state(_argsplit_state_t* state);

uint64_t _argsplit_count_chars_and_args(const char* str, uint64_t* argc, uint64_t* totalChars, uint64_t maxLen);

const char** _argsplit_backend(const char** argv, const char* str, uint64_t argc, uint64_t maxLen);

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>

typedef struct
{
    const char* current;
    uint8_t escaped;
    bool inQuote;
    bool newArg;
    bool first;
} _ArgsplitState_t;

#define _ARGSPLIR_CREATE(str) {str, 0, false, false, true}

static bool _ArgsplitIsNewArg(_ArgsplitState_t* state)
{
    return state->newArg;
}

static bool _ArgsplitStepState(_ArgsplitState_t* state)
{
    state->newArg = false;

    if (!state->first)
    {
        state->current++;
    }
    else
    {
        state->newArg = true;
    }
    state->first = false;

    while (true)
    {
        if (state->escaped != 0)
        {
            state->escaped--;
        }

        if (!state->escaped && !state->inQuote && isspace(*state->current))
        {
            state->newArg = true;
            while (isspace(*state->current))
            {
                state->current++;
            }
        }

        if (*state->current == '\\' && !state->escaped)
        {
            state->escaped = 2;
        }
        else if (*state->current == '"')
        {
            state->inQuote = !state->inQuote;
            state->newArg = true;
        }
        else if (*state->current == '\0')
        {
            return false;
        }
        else
        {
            return true;
        }

        state->current++;
    }
}

static uint64_t _ArgsplitCountCharsAndArgs(const char* str, uint64_t* argc, uint64_t* totalChars)
{
    *argc = 0;
    *totalChars = 0;

    _ArgsplitState_t state = _ARGSPLIR_CREATE(str);
    while (true)
    {
        if (!_ArgsplitStepState(&state))
        {
            break;
        }

        if (state.newArg)
        {
            (*argc)++;
        }
        totalChars++;
    }

    if (state.inQuote || state.escaped)
    {
        return UINT64_MAX;
    }

    return 0;
}

const char** argsplit(const char* str, uint64_t* count)
{
    while (isspace(*str))
    {
        str++;
    }

    uint64_t argc;
    uint64_t totalChars;
    if (_ArgsplitCountCharsAndArgs(str, &argc, &totalChars) == UINT64_MAX)
    {
        return NULL;
    }

    uint64_t argvSize = sizeof(char*) * (argc + 1);
    uint64_t stringsSize = totalChars + argc;

    char** argv = malloc(argvSize + stringsSize);
    if (argv == NULL)
    {
        return NULL;
    }

    char* strings = (char*)((uintptr_t)argv + argvSize);
    argv[0] = strings;
    argv[argc] = NULL;

    _ArgsplitState_t state = _ARGSPLIR_CREATE(str);
    uint64_t stringIndex = 0;
    char* out = strings;
    while (true)
    {
        if (!_ArgsplitStepState(&state))
        {
            break;
        }

        if (state.newArg)
        {
            if (out > strings && *(out - 1) != '\0')
            {
                *out++ = '\0';
            }
            argv[stringIndex++] = out;
        }
        *out++ = *state.current;
    }

    if (out > strings && *(out - 1) != '\0')
    {
        *out++ = '\0';
    }

    if (count != NULL)
    {
        *count = argc;
    }
    return (const char**)argv;
}
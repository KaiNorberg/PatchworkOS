#include "argsplit.h"

bool _argsplit_step_state(_argsplit_state_t* state)
{
    state->isNewArg = false;

    if (!state->isFirst)
    {
        state->current++;
        state->processedChars++;

        if (state->maxLen != 0 && state->processedChars >= state->maxLen)
        {
            return false;
        }
    }
    else
    {
        state->isNewArg = true;
    }
    state->isFirst = false;

    while (true)
    {
        if (state->escaped != 0)
        {
            state->escaped--;
        }

        if (!state->escaped && !state->inQuote && isspace(*state->current))
        {
            state->isNewArg = true;
            while (isspace(*state->current))
            {
                state->current++;
                state->processedChars++;
                if (state->maxLen != 0 && state->processedChars >= state->maxLen)
                {
                    return false;
                }
            }
        }

        if (*state->current == '\\' && !state->escaped)
        {
            state->escaped = 2;
        }
        else if (*state->current == '"')
        {
            state->inQuote = !state->inQuote;
            state->isNewArg = true;
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
        state->processedChars++;

        if (state->maxLen != 0 && state->processedChars >= state->maxLen)
        {
            return false;
        }
    }
}

uint64_t _argsplit_count_chars_and_args(const char* str, uint64_t* argc, uint64_t* totalChars, uint64_t maxLen)
{
    *argc = 0;
    *totalChars = 0;

    _argsplit_state_t state = _ARGSPLIT_CREATE(str, maxLen);
    while (true)
    {
        if (!_argsplit_step_state(&state))
        {
            break;
        }

        if (state.isNewArg)
        {
            (*argc)++;
        }
        (*totalChars)++;
    }

    if (state.inQuote || state.escaped)
    {
        return UINT64_MAX;
    }

    return 0;
}

const char** _argsplit_backend(const char** argv, const char* str, uint64_t argc, uint64_t maxLen)
{
    uint64_t argvSize = sizeof(char*) * (argc + 1);
    char* strings = (char*)((uintptr_t)argv + argvSize);
    argv[0] = strings;
    argv[argc] = NULL;

    _argsplit_state_t state = _ARGSPLIT_CREATE(str, maxLen);
    uint64_t stringIndex = 0;
    char* out = strings;
    while (true)
    {
        if (!_argsplit_step_state(&state))
        {
            break;
        }

        if (state.isNewArg)
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

    if (state.inQuote || state.escaped)
    {
        return NULL;
    }

    return (const char**)argv;
}

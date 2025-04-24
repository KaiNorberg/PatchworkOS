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
    uint64_t processedChars;
    uint64_t maxLen;
} _ArgsplitState_t;

#define _ARGSPLIT_CREATE(str, maxLen) {str, 0, false, false, true, 0, maxLen}

static bool _ArgsplitStepState(_ArgsplitState_t* state)
{
    state->newArg = false;

    if (!state->first)
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
        state->processedChars++;

        if (state->maxLen != 0 && state->processedChars >= state->maxLen)
        {
            return false;
        }
    }
}

static uint64_t _ArgsplitCountCharsAndArgs(const char* str, uint64_t* argc, uint64_t* totalChars, uint64_t maxLen)
{
    *argc = 0;
    *totalChars = 0;

    _ArgsplitState_t state = _ARGSPLIT_CREATE(str, maxLen);
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
        (*totalChars)++;
    }

    if (state.inQuote || state.escaped)
    {
        return UINT64_MAX;
    }

    return 0;
}

static const char** _ArgsplitBackend(const char** argv, const char* str, uint64_t argc, uint64_t maxLen)
{
    uint64_t argvSize = sizeof(char*) * (argc + 1);
    char* strings = (char*)((uintptr_t)argv + argvSize);
    argv[0] = strings;
    argv[argc] = NULL;

    _ArgsplitState_t state = _ARGSPLIT_CREATE(str, maxLen);
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

    if (state.inQuote || state.escaped)
    {
        return NULL;
    }

    return (const char**)argv;
}

const char** argsplit(const char* str, uint64_t maxLen, uint64_t* count)
{
    uint64_t skipped = 0;
    while (isspace(*str) && (maxLen == 0 || skipped < maxLen))
    {
        str++;
        skipped++;
    }
    maxLen = (maxLen == 0) ? 0 : (maxLen > skipped ? maxLen - skipped : 0);

    uint64_t argc;
    uint64_t totalChars;
    if (_ArgsplitCountCharsAndArgs(str, &argc, &totalChars, maxLen) == UINT64_MAX)
    {
        return NULL;
    }

    uint64_t argvSize = sizeof(char*) * (argc + 1);
    uint64_t stringsSize = totalChars + argc;

    const char** argv = malloc(argvSize + stringsSize);
    if (argv == NULL)
    {
        return NULL;
    }
    if (count != NULL)
    {
        *count = argc;
    }
    if (argc == 0)
    {
        argv[0] = NULL;
        return argv;
    }

    return _ArgsplitBackend(argv, str, argc, maxLen);
}

const char** argsplit_buf(void* buf, uint64_t size, const char* str, uint64_t maxLen, uint64_t* count)
{
    uint64_t skipped = 0;
    while (isspace(*str) && (maxLen == 0 || skipped < maxLen))
    {
        str++;
        skipped++;
    }
    maxLen = (maxLen == 0) ? 0 : (maxLen > skipped ? maxLen - skipped : 0);

    uint64_t argc;
    uint64_t totalChars;
    if (_ArgsplitCountCharsAndArgs(str, &argc, &totalChars, maxLen) == UINT64_MAX)
    {
        return NULL;
    }

    uint64_t argvSize = sizeof(char*) * (argc + 1);
    uint64_t stringsSize = totalChars + argc;

    const char** argv = buf;
    if (size < argvSize + stringsSize)
    {
        return NULL;
    }
    if (count != NULL)
    {
        *count = argc;
    }
    if (argc == 0)
    {
        argv[0] = NULL;
        return argv;
    }

    return _ArgsplitBackend(buf, str, argc, maxLen);
}

#pragma once

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

typedef struct
{
    const char* string;
} Token;

static inline Token token_first(const char* command)
{
    return (Token){.string = command};
}

static inline bool token_next(Token* token)
{
    const char* next = strchr(token->string, ' ');
    if (next == NULL || *(next + 1) == '\0')
    {
        return false;
    }

    token->string = next + 1;
    return true;
}

static inline uint64_t token_length(const Token* token)
{
    uint64_t i = 0;
    while (token->string[i] != '\0' && token->string[i] != ' ')
    {
        i++;
    }

    return i;
}

static inline uint64_t token_string(const Token* token, char* buffer, uint64_t length)
{
    uint64_t i = 0;
    for (; i < length - 2; i++)
    {
        if (token->string[i] == '\0' || token->string[i] == ' ')
        {
            buffer[i] = '\0';
            return 0;
        }

        buffer[i] = token->string[i];
    }

    errno = EBUFFER;
    return ERR;
}

static inline bool token_compare(const Token* token, const char* string)
{
    uint64_t length = token_length(token);
    if (length != strlen(string))
    {
        return false;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        if (token->string[i] != string[i])
        {
            return false;
        }
    }

    return true;
}
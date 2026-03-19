#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lisc.h>

typedef struct
{
    lisc_t* lisc;
    const char* input;
    size_t size;
    size_t index;
    uint16_t stack[LISC_MAX_DEPTH];
    size_t stackIndex;
} lisc_parse_t;

static void lisc_error(lisc_parse_t* ctx, const char* error)
{
    size_t line = 1;
    size_t column = 1;
    size_t lineStart = 0;
    for (size_t i = 0; i < ctx->index; i++)
    {
        if (ctx->input[i] == '\n')
        {
            line++;
            column = 1;
            lineStart = i + 1;
        }
        else
        {
            column++;
        }
    }

    int len =
        snprintf(ctx->lisc->error, sizeof(ctx->lisc->error), "lisc:%zu:%zu: %s\n  %4zu |", line, column, error, line);
    size_t i = lineStart;
    while (i < ctx->size && ctx->input[i] != '\n')
    {
        ctx->lisc->error[len++] = ctx->input[i];
        i++;
    }
    ctx->lisc->error[len++] = '\n';
    ctx->lisc->error[len++] = ' ';
    ctx->lisc->error[len++] = ' ';
    ctx->lisc->error[len++] = '|';
    for (size_t j = 0; j < column + 4; j++)
    {
        ctx->lisc->error[len++] = ' ';
    }
    ctx->lisc->error[len++] = '^';
    ctx->lisc->error[len++] = '\n';
    ctx->lisc->error[len] = '\0';

    if (ctx->lisc->items != ctx->lisc->small)
    {
        free(ctx->lisc->items);
    }
    ctx->lisc->count = 0;
    ctx->lisc->items = ctx->lisc->small;
    ctx->lisc->capacity = LISC_SMALL_MAX;
}

status_t lisc_init(lisc_t* lisc, const char* input, size_t size)
{
    if (lisc == NULL || input == NULL || size >= LISC_NONE)
    {
        return ERR(LIBSTD, INVAL);
    }

    lisc->input = input;
    lisc->items = lisc->small;
    lisc->count = 0;
    lisc->capacity = LISC_SMALL_MAX;

    lisc_parse_t ctx;
    ctx.lisc = lisc;
    ctx.input = input;
    ctx.size = size;
    ctx.index = 0;
    ctx.stackIndex = 0;

    while (true)
    {
        while (ctx.index < ctx.size && isspace(ctx.input[ctx.index]))
        {
            ctx.index++;
        }

        if (ctx.index >= ctx.size)
        {
            break;
        }

        if (lisc->count == lisc->capacity)
        {
            if (lisc->capacity == LISC_SMALL_MAX)
            {
                lisc_item_t* large = malloc(lisc->capacity * 2 * sizeof(lisc_item_t));
                if (large == NULL)
                {
                    lisc_error(&ctx, "out of memory");
                    return ERR(LIBSTD, NOMEM);
                }
                memcpy(large, lisc->small, lisc->capacity * sizeof(lisc_item_t));
                lisc->items = large;
                lisc->capacity *= 2;
            }
            else
            {
                lisc_item_t* large = realloc(lisc->items, lisc->capacity * 2 * sizeof(lisc_item_t));
                if (large == NULL)
                {
                    lisc_error(&ctx, "out of memory");
                    return ERR(LIBSTD, NOMEM);
                }
                lisc->items = large;
                lisc->capacity *= 2;
            }
        }

        switch (ctx.input[ctx.index])
        {
        case '(':
        {
            if (ctx.stackIndex == LISC_MAX_DEPTH)
            {
                lisc_error(&ctx, "too many nested expressions");
                return ERR(LIBSTD, INVAL);
            }

            if (ctx.stackIndex == 0 && ctx.index != 0)
            {
                lisc_error(&ctx, "unexpected '('");
                return ERR(LIBSTD, INVAL);
            }

            uint16_t childIndex = lisc->count++;
            lisc_item_t* item = &lisc->items[childIndex];
            item->next = LISC_NONE;
            item->type = LISC_LIST;
            item->list.first = LISC_NONE;
            item->list.last = LISC_NONE;

            if (ctx.stackIndex != 0)
            {
                uint16_t parent_index = ctx.stack[ctx.stackIndex - 1];
                lisc_item_t* parent = &lisc->items[parent_index];
                if (parent->list.first == LISC_NONE)
                {
                    parent->list.first = childIndex;
                }
                else
                {
                    lisc->items[parent->list.last].next = childIndex;
                }
                parent->list.last = childIndex;
            }

            ctx.stack[ctx.stackIndex++] = childIndex;
            ctx.index++;
        }
        break;
        case ')':
        {
            if (ctx.stackIndex == 0)
            {
                lisc_error(&ctx, "unexpected ')'");
                return ERR(LIBSTD, INVAL);
            }
            ctx.stackIndex--;
            ctx.index++;
        }
        break;
        default:
        {
            if (ctx.stackIndex == 0)
            {
                lisc_error(&ctx, "unexpected atom");
                return ERR(LIBSTD, INVAL);
            }

            uint16_t child_index = lisc->count++;
            lisc_item_t* item = &lisc->items[child_index];
            item->next = LISC_NONE;
            item->type = LISC_ATOM;

            uint16_t parent_index = ctx.stack[ctx.stackIndex - 1];
            lisc_item_t* parent = &lisc->items[parent_index];

            if (parent->list.first == LISC_NONE)
            {
                parent->list.first = child_index;
            }
            else
            {
                lisc->items[parent->list.last].next = child_index;
            }
            parent->list.last = child_index;

            if (ctx.input[ctx.index] == '"')
            {
                ctx.index++;
                item->atom.start = ctx.index;
                while (ctx.index < ctx.size && ctx.input[ctx.index] != '"')
                {
                    ctx.index++;
                }

                if (ctx.index >= ctx.size)
                {
                    lisc_error(&ctx, "missing '\"'");
                    return ERR(LIBSTD, INVAL);
                }

                item->atom.end = ctx.index;
                ctx.index++;
                break;
            }

            item->atom.start = ctx.index;
            while (ctx.index < ctx.size && !isspace(ctx.input[ctx.index]) && ctx.input[ctx.index] != '(' &&
                ctx.input[ctx.index] != ')' && ctx.input[ctx.index] != '"')
            {
                ctx.index++;
            }
            item->atom.end = ctx.index;

            if (ctx.index < ctx.size && ctx.input[item->atom.end] == '"')
            {
                lisc_error(&ctx, "unexpected '\"'");
                return ERR(LIBSTD, INVAL);
            }
        }
        break;
        }
    }

    if (ctx.stackIndex != 0)
    {
        lisc_error(&ctx, "missing ')'");
        return ERR(LIBSTD, INVAL);
    }

    return OK;
}
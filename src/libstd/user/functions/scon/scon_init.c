#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/scon.h>

typedef struct
{
    scon_t* scon;
    const char* input;
    size_t size;
    size_t index;
    uint16_t stack[SCON_MAX_DEPTH];
    size_t stackIndex;
} scon_parse_t;

static void scon_error(scon_parse_t* ctx, const char* error)
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
        snprintf(ctx->scon->error, sizeof(ctx->scon->error), "scon:%zu:%zu: %s\n  | %4zu | ", line, column, error, line);
    size_t i = lineStart;
    while (i < ctx->size && ctx->input[i] != '\n')
    {
        ctx->scon->error[len++] = ctx->input[i];
        i++;
    }
    ctx->scon->error[len++] = '\n';
    ctx->scon->error[len++] = ' ';
    ctx->scon->error[len++] = ' ';
    ctx->scon->error[len++] = '|';
    for (size_t j = 0; j < column + 6; j++)
    {
        ctx->scon->error[len++] = ' ';
    }
    ctx->scon->error[len++] = '^';
    ctx->scon->error[len] = '\0';

    if (ctx->scon->items != ctx->scon->small)
    {
        free(ctx->scon->items);
    }
    ctx->scon->count = 0;
    ctx->scon->items = ctx->scon->small;
    ctx->scon->capacity = SCON_SMALL_MAX;
}

status_t scon_init(scon_t* scon, const char* input, size_t size)
{
    if (scon == NULL || input == NULL || size >= SCON_NONE)
    {
        return ERR(LIBSTD, INVAL);
    }

    scon->input = input;
    scon->items = scon->small;
    scon->count = 0;
    scon->capacity = SCON_SMALL_MAX;

    scon_parse_t ctx;
    ctx.scon = scon;
    ctx.input = input;
    ctx.size = size;
    ctx.index = 0;
    ctx.stackIndex = 0;

    scon->count++;
    scon->items[0].next = SCON_NONE;
    scon->items[0].type = SCON_LIST;
    scon->items[0].list.first = SCON_NONE;
    scon->items[0].list.last = SCON_NONE;

    ctx.stack[ctx.stackIndex++] = 0;

    while (true)
    {
        while (ctx.index < ctx.size && isspace((unsigned char)ctx.input[ctx.index]))
        {
            ctx.index++;
        }

        if (ctx.index >= ctx.size)
        {
            break;
        }

        if (scon->count == scon->capacity)
        {
            if (scon->capacity == SCON_SMALL_MAX)
            {
                scon_item_t* large = malloc(scon->capacity * 2 * sizeof(scon_item_t));
                if (large == NULL)
                {
                    scon_error(&ctx, "out of memory");
                    return ERR(LIBSTD, NOMEM);
                }
                memcpy(large, scon->small, scon->capacity * sizeof(scon_item_t));
                scon->items = large;
                scon->capacity *= 2;
            }
            else
            {
                scon_item_t* large = realloc(scon->items, scon->capacity * 2 * sizeof(scon_item_t));
                if (large == NULL)
                {
                    scon_error(&ctx, "out of memory");
                    return ERR(LIBSTD, NOMEM);
                }
                scon->items = large;
                scon->capacity *= 2;
            }
        }

        switch (ctx.input[ctx.index])
        {
        case '(':
        {
            if (ctx.stackIndex == SCON_MAX_DEPTH)
            {
                scon_error(&ctx, "too many nested expressions");
                return ERR(LIBSTD, INVALSCON);
            }

            if (ctx.stackIndex <= 1 && scon->items[0].list.first != SCON_NONE)
            {
                scon_error(&ctx, "unexpected '('");
                return ERR(LIBSTD, INVALSCON);
            }

            if (scon->count >= SCON_NONE)
            {
                scon_error(&ctx, "too many items");
                return ERR(LIBSTD, INVALSCON);
            }

            uint16_t childIndex = scon->count++;
            scon_item_t* item = &scon->items[childIndex];
            item->next = SCON_NONE;
            item->type = SCON_LIST;
            item->list.first = SCON_NONE;
            item->list.last = SCON_NONE;

            uint16_t parent_index = ctx.stack[ctx.stackIndex - 1];
            scon_item_t* parent = &scon->items[parent_index];
            if (parent->list.first == SCON_NONE)
            {
                parent->list.first = childIndex;
            }
            else
            {
                scon->items[parent->list.last].next = childIndex;
            }
            parent->list.last = childIndex;

            ctx.stack[ctx.stackIndex++] = childIndex;
            ctx.index++;
        }
        break;
        case ')':
        {
            if (ctx.stackIndex <= 1)
            {
                scon_error(&ctx, "unexpected ')'");
                return ERR(LIBSTD, INVALSCON);
            }
            ctx.stackIndex--;
            ctx.index++;
        }
        break;
        default:
        {
            if (ctx.stackIndex <= 1)
            {
                scon_error(&ctx, "unexpected atom");
                return ERR(LIBSTD, INVALSCON);
            }

            if (scon->count >= SCON_NONE)
            {
                scon_error(&ctx, "too many items");
                return ERR(LIBSTD, INVALSCON);
            }

            uint16_t childIndex = scon->count++;
            scon_item_t* item = &scon->items[childIndex];
            item->next = SCON_NONE;
            item->type = SCON_ATOM;

            uint16_t parentIndex = ctx.stack[ctx.stackIndex - 1];
            scon_item_t* parent = &scon->items[parentIndex];

            if (parent->list.first == SCON_NONE)
            {
                parent->list.first = childIndex;
            }
            else
            {
                scon->items[parent->list.last].next = childIndex;
            }
            parent->list.last = childIndex;

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
                    scon_error(&ctx, "missing '\"'");
                    return ERR(LIBSTD, INVALSCON);
                }

                item->atom.end = ctx.index;
                ctx.index++;
                break;
            }

            item->atom.start = ctx.index;
            while (ctx.index < ctx.size && !isspace((unsigned char)ctx.input[ctx.index]) && ctx.input[ctx.index] != '(' &&
                ctx.input[ctx.index] != ')' && ctx.input[ctx.index] != '"')
            {
                ctx.index++;
            }
            item->atom.end = ctx.index;

            if (ctx.index < ctx.size && ctx.input[item->atom.end] == '"')
            {
                scon_error(&ctx, "unexpected '\"'");
                return ERR(LIBSTD, INVALSCON);
            }
        }
        break;
        }
    }

    if (ctx.stackIndex > 1)
    {
        scon_error(&ctx, "missing ')'");
        return ERR(LIBSTD, INVALSCON);
    }

    return OK;
}
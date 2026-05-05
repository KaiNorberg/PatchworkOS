#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scon_priv.h"

typedef struct
{
    scon_t* scon;
    const char* input;
    size_t size;
    size_t index;
    scon_item_t* stack[SCON_MAX_DEPTH];
    size_t stackIndex;
} scon_parse_t;

static scon_item_t* scon_item_alloc(scon_t* ctx)
{
    if (ctx->freeList != NULL)
    {
        scon_item_t* item = ctx->freeList;
        ctx->freeList = item->next;
        memset(item, 0, sizeof(scon_item_t));
        return item;
    }

    scon_block_t* block = NULL;
    if (!list_is_empty(&ctx->blocks))
    {
        block = CONTAINER_OF(list_last(&ctx->blocks), scon_block_t, link);
    }

    if (block == NULL || block->used >= SCON_ARENA_BLOCK_SIZE)
    {
        block = malloc(sizeof(scon_block_t));
        if (block == NULL)
        {
            return NULL;
        }
        list_entry_init(&block->link);
        block->used = 0;
        list_push_back(&ctx->blocks, &block->link);
    }

    scon_item_t* item = &block->items[block->used++];
    memset(item, 0, sizeof(scon_item_t));
    return item;
}

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

    int len = snprintf(ctx->scon->error, sizeof(ctx->scon->error), "scon:%zu:%zu: %s\n  | %4zu | ", line, column, error,
        line);
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
}

const char* scon_error_msg(scon_t* scon)
{
    if (scon == NULL)
    {
        return "";
    }
    return scon->error;
}

status_t scon_parse(const char* input, size_t size, scon_t** out)
{
    if (out == NULL || input == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    scon_t* scon = malloc(sizeof(scon_t));
    if (scon == NULL)
    {
        return ERR(LIBSTD, NOMEM);
    }

    scon->input = input;
    scon->root = NULL;
    list_init(&scon->blocks);
    scon->freeList = NULL;
    scon->error[0] = '\0';

    *out = scon;

    scon_parse_t ctx;
    ctx.scon = scon;
    ctx.input = input;
    ctx.size = size;
    ctx.index = 0;
    ctx.stackIndex = 0;

    scon_item_t* root = scon_item_alloc(scon);
    if (root == NULL)
    {
        return ERR(LIBSTD, NOMEM);
    }
    root->type = SCON_LIST;
    root->sourceIndex = 0;
    scon->root = root;

    ctx.stack[ctx.stackIndex++] = root;

    while (true)
    {
        while (ctx.index < ctx.size && isspace((unsigned char)ctx.input[ctx.index]))
        {
            ctx.index++;
        }

        if (ctx.index < ctx.size && ctx.input[ctx.index] == ';')
        {
            while (ctx.index < ctx.size && ctx.input[ctx.index] != '\n')
            {
                ctx.index++;
            }
            continue;
        }

        if (ctx.index >= ctx.size)
        {
            break;
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

            if (ctx.stackIndex <= 1 && scon->root->list.first != NULL)
            {
                scon_error(&ctx, "unexpected '('");
                return ERR(LIBSTD, INVALSCON);
            }

            scon_item_t* child = scon_item_alloc(scon);
            if (child == NULL)
            {
                scon_error(&ctx, "out of memory");
                return ERR(LIBSTD, NOMEM);
            }
            child->type = SCON_LIST;
            child->sourceIndex = ctx.index;

            scon_item_t* parent = ctx.stack[ctx.stackIndex - 1];
            if (parent->list.first == NULL)
            {
                parent->list.first = child;
            }
            else
            {
                parent->list.last->next = child;
            }
            parent->list.last = child;

            ctx.stack[ctx.stackIndex++] = child;
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

            scon_item_t* child = scon_item_alloc(scon);
            if (child == NULL)
            {
                scon_error(&ctx, "out of memory");
                return ERR(LIBSTD, NOMEM);
            }
            child->type = SCON_ATOM;
            child->sourceIndex = ctx.index;

            scon_item_t* parent = ctx.stack[ctx.stackIndex - 1];

            if (parent->list.first == NULL)
            {
                parent->list.first = child;
            }
            else
            {
                parent->list.last->next = child;
            }
            parent->list.last = child;

            if (ctx.input[ctx.index] == '"')
            {
                ctx.index++;
                child->atom.str = &ctx.input[ctx.index];
                size_t start = ctx.index;
                while (ctx.index < ctx.size && ctx.input[ctx.index] != '"')
                {
                    ctx.index++;
                }

                if (ctx.index >= ctx.size)
                {
                    scon_error(&ctx, "missing '\"'");
                    return ERR(LIBSTD, INVALSCON);
                }

                child->atom.length = ctx.index - start;
                ctx.index++;
                break;
            }

            child->atom.str = &ctx.input[ctx.index];
            size_t start = ctx.index;
            while (ctx.index < ctx.size && !isspace((unsigned char)ctx.input[ctx.index]) &&
                ctx.input[ctx.index] != '(' && ctx.input[ctx.index] != ')' && ctx.input[ctx.index] != '"')
            {
                ctx.index++;
            }
            child->atom.length = ctx.index - start;

            if (ctx.index < ctx.size && ctx.input[ctx.index] == '"')
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
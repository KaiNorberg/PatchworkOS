#include <stdio.h>
#include <stdlib.h>

#include "common/print.h"
#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

typedef struct
{
    FILE* stream;
    char buffer[MAX_PATH];
    uint64_t count;
    uint64_t total;
} vfprintf_ctx_t;

static void vfprintf_put_func(char chr, void* context)
{
    vfprintf_ctx_t* ctx = (vfprintf_ctx_t*)context;

    if (ctx->count >= MAX_PATH)
    {
        uint64_t result = fwrite(ctx->buffer, 1, ctx->count, ctx->stream);
        if (result != ERR)
        {
            ctx->total += ctx->count;
        }
        ctx->count = 0;
    }
    ctx->buffer[ctx->count++] = chr;
}

int vfprintf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg)
{
    vfprintf_ctx_t ctx = {
        .stream = stream,
        .count = 0,
        .total = 0,
    };

    _Print(vfprintf_put_func, &ctx, format, arg);
    if (ctx.count > 0)
    {
        uint64_t result = fwrite(ctx.buffer, 1, ctx.count, ctx.stream);
        if (result != ERR)
        {
            ctx.total += ctx.count;
        }
        ctx.count = 0;
    }
    return ctx.total;
}

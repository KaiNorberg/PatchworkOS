#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

typedef struct
{
    char* buffer;
    size_t maxSize;
    size_t count;
} snprtinf_ctx_t;

static void snprintf_put_func(char chr, void* context)
{
    snprtinf_ctx_t* ctx = (snprtinf_ctx_t*)context;
    if (ctx->count < ctx->maxSize - 1)
    {
        ctx->buffer[ctx->count] = chr;
    }
    ctx->count++;
}

int snprintf(char* _RESTRICT buffer, size_t size, const char* _RESTRICT format, ...)
{
    snprtinf_ctx_t ctx = {buffer, size, 0};
    va_list args;
    va_start(args, format);
    int result = _Print(snprintf_put_func, &ctx, format, args);
    va_end(args);

    if (size > 0)
    {
        buffer[ctx.count < size - 1 ? ctx.count : size - 1] = '\0';
    }

    return result;
}

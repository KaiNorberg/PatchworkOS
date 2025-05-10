#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

typedef struct
{
    char* buffer;
    size_t maxSize;
    size_t count;
} vsnprintf_ctx_t;

void vsnprintf_put_func(char chr, void* context)
{
    vsnprintf_ctx_t* ctx = (vsnprintf_ctx_t*)context;
    if (ctx->count < ctx->maxSize - 1)
    {
        ctx->buffer[ctx->count] = chr;
    }
    ctx->count++;
}

int vsnprintf(char* _RESTRICT buffer, size_t size, const char* _RESTRICT format, va_list args)
{
    vsnprintf_ctx_t ctx = {buffer, size, 0};
    int result = _Print(vsnprintf_put_func, &ctx, format, args);

    if (size > 0)
    {
        buffer[ctx.count < size - 1 ? ctx.count : size - 1] = '\0';
    }

    return result;
}

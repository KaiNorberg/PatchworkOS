#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

typedef struct
{
    char* buffer;
    size_t count;
} sprintf_ctx_t;

static void sprintf_put_func(char chr, void* context)
{
    sprintf_ctx_t* ctx = (sprintf_ctx_t*)context;
    ctx->buffer[ctx->count++] = chr;
}

int sprintf(char* _RESTRICT buffer, const char* _RESTRICT format, ...)
{
    sprintf_ctx_t ctx = {buffer, 0};
    va_list args;
    va_start(args, format);
    int result = _Print(sprintf_put_func, &ctx, format, args);
    va_end(args);

    buffer[result] = '\0';

    return result;
}

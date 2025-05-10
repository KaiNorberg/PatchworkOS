#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

typedef struct
{
    char* buffer;
    size_t count;
} vsprintf_ctx_t;

static void vsprintf_put_func(char chr, void* context)
{
    vsprintf_ctx_t* ctx = (vsprintf_ctx_t*)context;
    ctx->buffer[ctx->count++] = chr;
}

int vsprintf(char* _RESTRICT buffer, const char* _RESTRICT format, va_list args)
{
    vsprintf_ctx_t ctx = {buffer, 0};
    int result = _Print(vsprintf_put_func, &ctx, format, args);

    buffer[result] = '\0';

    return result;
}

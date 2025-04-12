#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"

// TODO: Implement streams!

int sprintf(char* _RESTRICT buffer, const char* _RESTRICT format, ...)
{
    typedef struct
    {
        char* buffer;
        size_t count;
    } sprintf_ctx_t;

    void put_func(char chr, void* context)
    {
        sprintf_ctx_t* ctx = (sprintf_ctx_t*)context;
        ctx->buffer[ctx->count++] = chr;
    }

    sprintf_ctx_t ctx = {buffer, 0};
    va_list args;
    va_start(args, format);
    int result = _Print(put_func, &ctx, format, args);
    va_end(args);

    buffer[result] = '\0';

    return result;
}

int vsprintf(char* _RESTRICT buffer, const char* _RESTRICT format, va_list args)
{
    typedef struct
    {
        char* buffer;
        size_t count;
    } vsprintf_ctx_t;

    void put_func(char chr, void* context)
    {
        vsprintf_ctx_t* ctx = (vsprintf_ctx_t*)context;
        ctx->buffer[ctx->count++] = chr;
    }

    vsprintf_ctx_t ctx = {buffer, 0};
    int result = _Print(put_func, &ctx, format, args);

    buffer[result] = '\0';

    return result;
}

int snprintf(char* _RESTRICT buffer, size_t size, const char* _RESTRICT format, ...)
{
    typedef struct
    {
        char* buffer;
        size_t maxSize;
        size_t count;
    } snprtinf_ctx_t;

    void put_func(char chr, void* context)
    {
        snprtinf_ctx_t* ctx = (snprtinf_ctx_t*)context;
        if (ctx->count < ctx->maxSize - 1)
        {
            ctx->buffer[ctx->count] = chr;
        }
        ctx->count++;
    }

    snprtinf_ctx_t ctx = {buffer, size, 0};
    va_list args;
    va_start(args, format);
    int result = _Print(put_func, &ctx, format, args);
    va_end(args);

    if (size > 0)
    {
        buffer[ctx.count < size - 1 ? ctx.count : size - 1] = '\0';
    }

    return result;
}

int vsnprintf(char* _RESTRICT buffer, size_t size, const char* _RESTRICT format, va_list args)
{
    typedef struct
    {
        char* buffer;
        size_t maxSize;
        size_t count;
    } vsnprintf_ctx_t;

    void put_func(char chr, void* context)
    {
        vsnprintf_ctx_t* ctx = (vsnprintf_ctx_t*)context;
        if (ctx->count < ctx->maxSize - 1)
        {
            ctx->buffer[ctx->count] = chr;
        }
        ctx->count++;
    }

    vsnprintf_ctx_t ctx = {buffer, size, 0};
    int result = _Print(put_func, &ctx, format, args);

    if (size > 0)
    {
        buffer[ctx.count < size - 1 ? ctx.count : size - 1] = '\0';
    }

    return result;
}

char* asprintf(const char* _RESTRICT format, ...)
{
    va_list sizeArgs;
    va_start(sizeArgs, format);
    int size = vsnprintf(NULL, 0, format, sizeArgs);
    va_end(sizeArgs);

    if (size < 0)
    {
        return NULL;
    }

    char* buffer = (char*)malloc(size + 1);
    if (!buffer)
    {
        return NULL;
    }

    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    return buffer;
}

char* vasprintf(const char* _RESTRICT format, va_list args)
{
    va_list argsCopy;
    va_copy(argsCopy, args);

    int size = vsnprintf(NULL, 0, format, argsCopy);
    va_end(argsCopy);

    if (size < 0)
    {
        return NULL;
    }

    char* buffer = (char*)malloc(size + 1);
    if (!buffer)
    {
        return NULL;
    }

    vsprintf(buffer, format, args);

    return buffer;
}

int printf(const char* _RESTRICT format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

int vprintf(const char* _RESTRICT format, va_list args)
{
    return _PlatformVprintf(format, args);
}
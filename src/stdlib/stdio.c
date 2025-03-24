#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"

// TODO: Implement streams! All these implementations will need to be scrapped when streams are implemented.

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

#ifndef __KERNEL__

int printf(const char* _RESTRICT format, ...)
{
    void put_func(char chr, void* context)
    {
        write(STDOUT_FILENO, &chr, 1);
    }

    va_list args;
    va_start(args, format);
    int result = _Print(put_func, NULL, format, args);
    va_end(args);
    return result;
}

int vprintf(const char* _RESTRICT format, va_list args)
{
    void put_func(char chr, void* context)
    {
        write(STDOUT_FILENO, &chr, 1);
    }

    return _Print(put_func, NULL, format, args);
}

int dprintf(fd_t fd, const char* _RESTRICT format, ...)
{
    typedef struct
    {
        fd_t fd;
    } dprintf_ctx_t;

    void put_func(char chr, void* context)
    {
        dprintf_ctx_t* ctx = (dprintf_ctx_t*)context;
        write(ctx->fd, &chr, 1);
    }

    dprintf_ctx_t ctx = {fd};
    va_list args;
    va_start(args, format);
    int result = _Print(put_func, &ctx, format, args);
    va_end(args);

    return result;
}

int vdprintf(fd_t fd, const char* _RESTRICT format, va_list args)
{
    typedef struct
    {
        fd_t fd;
    } vdprintf_ctx_t;

    void put_func(char chr, void* context)
    {
        vdprintf_ctx_t* ctx = (vdprintf_ctx_t*)context;
        write(ctx->fd, &chr, 1);
    }

    vdprintf_ctx_t ctx = {fd};
    return _Print(put_func, &ctx, format, args);
}

#endif // #ifndef __KERNEL__

#ifdef __KERNEL__

#include "log.h"
#include "systime.h"

int printf(const char* _RESTRICT format, ...)
{
    char buffer[MAX_PATH];

    nsec_t time = log_time_enabled() ? systime_uptime() : 0;
    nsec_t sec = time / SEC;
    nsec_t ms = (time % SEC) / (SEC / 1000);

    va_list args;
    va_start(args, format);
    int result = vsprintf(buffer + sprintf(buffer, "[%10llu.%03llu] ", sec, ms), format, args);
    va_end(args);

    log_write(buffer);
    log_write("\n");
    return result + 1;
}

int vprintf(const char* _RESTRICT format, va_list args)
{
    char buffer[MAX_PATH];

    nsec_t time = log_time_enabled() ? systime_uptime() : 0;
    nsec_t sec = time / SEC;
    nsec_t ms = (time % SEC) / (SEC / 1000);

    uint64_t result = vsprintf(buffer + sprintf(buffer, "[%10llu.%03llu] ", sec, ms), format, args);

    log_write(buffer);
    log_write("\n");
    return result + 1;
}

#endif // #ifdef __KERNEL__
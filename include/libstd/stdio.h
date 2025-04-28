#ifndef _STDIO_H
#define _STDIO_H 1

#include <stdarg.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"
#include "_AUX/fd_t.h"
#include "_AUX/size_t.h"

int sprintf(char* _RESTRICT s, const char* _RESTRICT format, ...);
int vsprintf(char* _RESTRICT s, const char* _RESTRICT format, va_list args);
int snprintf(char* _RESTRICT s, size_t size, const char* _RESTRICT format, ...);
int vsnprintf(char* _RESTRICT s, size_t size, const char* _RESTRICT format, va_list args);
char* asprintf(const char* _RESTRICT format, ...);
char* vasprintf(const char* _RESTRICT format, va_list args);

int printf(const char* _RESTRICT format, ...);
int vprintf(const char* _RESTRICT format, va_list args);

#if defined(__cplusplus)
}
#endif

#endif

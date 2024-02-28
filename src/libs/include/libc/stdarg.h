#ifndef _STDARG_H
#define _STDARG_H 1

#ifdef __cplusplus
extern "C" {
#endif

typedef __builtin_va_list va_list;

#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#define va_end(ap) __builtin_va_end(ap)
#define va_start(ap, parmN) __builtin_va_start(ap, parmN)

#ifdef __cplusplus
}
#endif

#endif
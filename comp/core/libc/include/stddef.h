#ifndef _STDDEF_H
#define _STDDEF_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libc/NULL.h"
#include "_libc/config.h"
#include "_libc/ptrdiff_t.h"
#include "_libc/size_t.h"
#include "_libc/wchar_t.h"

#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#if _USE_ANNEX_K == 1
#include "_libc/rsize_t.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif

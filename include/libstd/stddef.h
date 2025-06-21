#ifndef _STDDEF_H
#define _STDDEF_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/ERR.h"
#include "_internal/NULL.h"
#include "_internal/config.h"
#include "_internal/ptrdiff_t.h"
#include "_internal/size_t.h"
#include "_internal/wchar_t.h"

#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#if _USE_ANNEX_K == 1
#include "_internal/rsize_t.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif

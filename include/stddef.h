#ifndef _STDDEF_H
#define _STDDEF_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/_FAIL.h"
#include "_libstd/NULL.h"
#include "_libstd/config.h"
#include "_libstd/ptrdiff_t.h"
#include "_libstd/size_t.h"
#include "_libstd/wchar_t.h"

#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#if _USE_ANNEX_K == 1
#include "_libstd/rsize_t.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif

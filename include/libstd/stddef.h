#ifndef _STDDEF_H
#define _STDDEF_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/ptrdiff_t.h"
#include "_AUX/size_t.h"
#include "_AUX/wchar_t.h"

#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#if _USE_ANNEX_K == 1
#include "_AUX/rsize_t.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif

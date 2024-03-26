#ifndef _STDDEF_H
#define _STDDEF_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "auxiliary/size_t.h"
#include "auxiliary/NULL.h"
#include "auxiliary/ERR.h"

typedef size_t rsize_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __WCHAR_TYPE__ wchar_t;

#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#if defined(__cplusplus)
}
#endif

#endif
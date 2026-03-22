#ifndef _STRINGS_H
#define _STRINGS_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/NULL.h"
#include "_libstd/config.h"
#include "_libstd/size_t.h"

int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, size_t n);

#if defined(__cplusplus)
}
#endif

#endif

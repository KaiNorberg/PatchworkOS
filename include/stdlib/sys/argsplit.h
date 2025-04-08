#ifndef _SYS_ARGSPLIT_H
#define _SYS_ARGSPLIT_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"

const char** argsplit(const char* str, uint64_t* count);

const char** argsplit_buf(void* buf, uint64_t size, const char* str, uint64_t* count);

#if defined(__cplusplus)
}
#endif

#endif

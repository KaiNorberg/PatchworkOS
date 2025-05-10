#pragma once

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <time.h>

#ifdef __KERNEL__
#include "kernel/platform.h"
#else
#include "user/platform.h"
#endif

void _PlatformEarlyInit(void);

void _PlatformLateInit(void);

void* _PlatformPageAlloc(uint64_t amount);

int* _PlatformErrnoFunc(void);

int _PlatformVprintf(const char* _RESTRICT format, va_list args);

void _PlatformAbort(void);

#pragma once

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <time.h>

#ifdef __KERNEL__
#include "kernel/platform.h"
#elif __BOOTLOADER__
#include "bootloader/platform.h"
#else
#include "user/platform.h"
#endif

void _PlatformEarlyInit(void);

void _PlatformLateInit(void);

void* _PlatformPageAlloc(uint64_t amount);

int* _PlatformErrnoFunc(void);

// User platform will ignore message.
_NORETURN void _PlatformAbort(const char* message);

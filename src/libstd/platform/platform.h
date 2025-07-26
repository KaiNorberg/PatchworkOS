#pragma once

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <time.h>

#ifdef __KERNEL__
#include "kernel/platform.h"
#elif __BOOT__
#include "boot/platform.h"
#else
#include "user/platform.h"
#endif

void _platform_early_init(void);

void _platform_late_init(void);

int* _platform_errno_get(void);

// User platform will ignore message.
_NORETURN void _platform_abort(const char* message);

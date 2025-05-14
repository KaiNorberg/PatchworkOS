#pragma once

#include "format.h"

#include <stdarg.h>
#include <stdint.h>

#include "platform/platform.h"

#if _PLATFORM_HAS_IO == 1
#define _PRINT_TEST(format, ...) fprintf(stderr, format __VA_OPT__(, ) __VA_ARGS__)
#else
#define _PRINT_TEST(format, ...)
#endif

const char* _Print(const char* spec, _FormatCtx_t* ctx);

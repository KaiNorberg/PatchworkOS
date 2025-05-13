#pragma once

#include "format.h"

#include <stdarg.h>
#include <stdint.h>

#include "platform/platform.h"

const char* _Print(const char* spec, _FormatCtx_t* ctx);

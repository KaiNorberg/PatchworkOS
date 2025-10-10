#pragma once

#include "encoding/term.h"

void aml_debug_error_print(aml_term_list_ctx_t* ctx, const char* function, const char* format, ...);

#define AML_DEBUG_ERROR(ctx, format, ...) aml_debug_error_print(ctx, __func__, format, ##__VA_ARGS__)

#pragma once

#include "aml_state.h"
#include "aml_value.h"
#include "log/log.h"

void aml_debug_error_print(aml_state_t* state, const char* function, const char* format, ...);

#define AML_DEBUG_ERROR(state, format, ...) aml_debug_error_print(state, __func__, format, ##__VA_ARGS__)

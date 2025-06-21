#pragma once
#include "use_annex_k.h"
#include <stdlib.h>

#include "error_strings.h"

extern constraint_handler_t _constraintHandler;

#define _CONSTRAINT_VIOLATION(e) _error_strings[e], NULL, e

void _constraint_handler_init(void);

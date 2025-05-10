#pragma once
#include "use_annex_k.h"
#include <stdlib.h>

#include "error_strings.h"

extern constraint_handler_t _ConstraintHandler;

#define _CONSTRAINT_VIOLATION(e) _ErrorStrings[e], NULL, e

void _ConstraintHandlerInit(void);

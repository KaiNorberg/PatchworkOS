#include "constraint_handler.h"

constraint_handler_t _constraintHandler;

void _constraint_handler_init(void)
{
    _constraintHandler = abort_handler_s;
}

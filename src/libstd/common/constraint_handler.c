#include "constraint_handler.h"

constraint_handler_t _ConstraintHandler;

void _ConstraintHandlerInit(void)
{
    _ConstraintHandler = abort_handler_s;
}

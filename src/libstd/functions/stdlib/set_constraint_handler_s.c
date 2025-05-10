#include "common/use_annex_k.h"
#include <stdlib.h>

#include "common/constraint_handler.h"

constraint_handler_t set_constraint_handler_s(constraint_handler_t handler)
{
    constraint_handler_t previous = _ConstraintHandler;

    if (handler == NULL)
    {
        _ConstraintHandler = abort_handler_s;
    }
    else
    {
        _ConstraintHandler = handler;
    }

    return previous;
}

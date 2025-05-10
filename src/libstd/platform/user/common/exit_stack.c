#include "exit_stack.h"

#include <errno.h>
#include <stdint.h>

static void (*_Stack[_EXIT_STACK_SIZE])(void);
static uint64_t _Length = 0;

void _ExitStackInit(void)
{
    _Length = 0;
}

uint64_t _ExitStackPush(void (*func)(void))
{
    if (_Length == _EXIT_STACK_SIZE)
    {
        return ERR;
    }
    else
    {
        _Stack[_Length++] = func;
        return 0;
    }
}

void _ExitStackDispatch(void)
{
    while (_Length != 0)
    {
        _Stack[--_Length]();
    }
}

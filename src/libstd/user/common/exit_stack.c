#include "exit_stack.h"

#include <errno.h>
#include <stdint.h>

static void (*_stack[_EXIT_STACK_SIZE])(void);
static uint64_t _length = 0;

void _exit_stack_init(void)
{
    _length = 0;
}

uint64_t _exit_stack_push(void (*func)(void))
{
    if (_length == _EXIT_STACK_SIZE)
    {
        return ERR;
    }
    else
    {
        _stack[_length++] = func;
        return 0;
    }
}

void _exit_stack_dispatch(void)
{
    while (_length != 0)
    {
        _stack[--_length]();
    }
}

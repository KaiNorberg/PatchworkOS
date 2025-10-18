#include "pmm_stack.h"

#include <sys/math.h>

void pmm_stack_init(pmm_stack_t* stack)
{
    stack->last = NULL;
    stack->index = 0;
    stack->free = 0;
}

void* pmm_stack_alloc(pmm_stack_t* stack)
{
    if (stack->last == NULL)
    {
        return NULL;
    }

    void* address;
    if (stack->index == 0)
    {
        address = stack->last;
        stack->last = stack->last->prev;
        stack->index = PMM_BUFFER_MAX;
    }
    else
    {
        address = stack->last->pages[--stack->index];
    }

    stack->free--;

    return address;
}

void pmm_stack_free(pmm_stack_t* stack, void* address)
{
    address = (void*)ROUND_DOWN(address, PAGE_SIZE);

    if (stack->last == NULL)
    {
        stack->last = address;
        stack->last->prev = NULL;
        stack->index = 0;
    }
    else if (stack->index == PMM_BUFFER_MAX)
    {
        page_buffer_t* next = address;
        next->prev = stack->last;
        stack->last = next;
        stack->index = 0;
    }
    else
    {
        stack->last->pages[stack->index++] = address;
    }

    stack->free++;
}

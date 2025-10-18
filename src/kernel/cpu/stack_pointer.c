#include "stack_pointer.h"

#include "log/log.h"
#include "mem/vmm.h"
#include "sched/thread.h"

uint64_t stack_pointer_init(stack_pointer_t* stack, uintptr_t maxAddress, uint64_t maxPages)
{
    if (stack == NULL || maxPages == 0 || !VMM_IS_PAGE_ALIGNED(maxAddress))
    {
        errno = EINVAL;
        return ERR;
    }

    stack->top = maxAddress;
    stack->bottom = stack->top - (maxPages * PAGE_SIZE);
    if (stack->bottom >= stack->top) // Overflow
    {
        errno = EOVERFLOW;
        return ERR;
    }
    stack->guardTop = stack->bottom;
    stack->guardBottom = stack->guardTop - (STACK_POINTER_GUARD_PAGES * PAGE_SIZE);
    if (stack->guardBottom >= stack->guardTop) // Overflow
    {
        errno = EOVERFLOW;
        return ERR;
    }
    stack->lastPageFault = 0;

    return 0;
}

uint64_t stack_pointer_init_buffer(stack_pointer_t* stack, void* buffer, uint64_t pages)
{
    if (stack == NULL || buffer == NULL || pages == 0 || !VMM_IS_PAGE_ALIGNED(buffer))
    {
        errno = EINVAL;
        return ERR;
    }

    stack->top = (uintptr_t)buffer + pages * PAGE_SIZE;
    stack->bottom = (uintptr_t)buffer;
    if (stack->bottom >= stack->top) // Overflow
    {
        errno = EOVERFLOW;
        return ERR;
    }
    // No guard pages when using a buffer.
    stack->guardTop = stack->bottom;
    stack->guardBottom = stack->bottom;
    stack->lastPageFault = 0;

    memset(buffer, 0, pages * PAGE_SIZE);

    return 0;
}

void stack_pointer_deinit(stack_pointer_t* stack, thread_t* thread)
{
    if (stack == NULL)
    {
        return;
    }

    vmm_unmap(&thread->process->space, (void*)stack->bottom, BYTES_TO_PAGES(stack->top - stack->bottom));

    stack->top = 0;
    stack->bottom = 0;
    stack->guardTop = 0;
    stack->guardBottom = 0;
    stack->lastPageFault = 0;
}

void stack_pointer_deinit_buffer(stack_pointer_t* stack)
{
    if (stack == NULL)
    {
        return;
    }

    stack->top = 0;
    stack->bottom = 0;
    stack->guardTop = 0;
    stack->guardBottom = 0;
    stack->lastPageFault = 0;
}

uint64_t stack_pointer_handle_page_fault(stack_pointer_t* stack, thread_t* thread, uintptr_t faultAddr,
    pml_flags_t flags)
{
    if (stack == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (faultAddr > PML_LOWER_HALF_END && faultAddr < PML_HIGHER_HALF_START)
    {
        LOG_ERR("Stack page fault at non-canonical address %p\n", (void*)faultAddr);
        errno = EFAULT;
        return ERR;
    }

    if (faultAddr >= stack->guardBottom && faultAddr < stack->guardTop)
    {
        LOG_ERR("Stack overflow detected at address %p\n", (void*)faultAddr);
        errno = EFAULT;
        return ERR;
    }

    if (faultAddr < stack->bottom || faultAddr >= stack->top)
    {
        errno = ENOENT;
        return ERR;
    }

    uintptr_t pageAlignedAddr = ROUND_DOWN(faultAddr, PAGE_SIZE);
    if (stack->lastPageFault == pageAlignedAddr)
    {
        LOG_ERR("Stack page fault loop detected at address %p\n", (void*)faultAddr);
        errno = EFAULT;
        return ERR;
    }
    stack->lastPageFault = pageAlignedAddr;

    // We cant block here since we are in an exception, so make sure the mutex is acquired before allocating.
    rwmutex_write_spin_acquire(&thread->process->space.mutex);
    if (vmm_alloc(&thread->process->space, (void*)pageAlignedAddr, PAGE_SIZE, flags) == NULL)
    {
        rwmutex_write_release(&thread->process->space.mutex);
        errno = ENOMEM;
        return ERR;
    }
    rwmutex_write_release(&thread->process->space.mutex);

    memset((void*)pageAlignedAddr, 0, PAGE_SIZE);

    return 0;
}

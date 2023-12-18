#include "context.h"

#include "string/string.h"
#include "page_allocator/page_allocator.h"
#include "heap/heap.h"
#include "gdt/gdt.h"

extern uint64_t _kernelStart;
extern uint64_t _kernelEnd;

Context* context_new(void* instructionPointer, uint64_t codeSegment, uint64_t stackSegment, uint64_t rFlags)
{
    Context* context = kmalloc(sizeof(Context));
    memset(context, 0, sizeof(Context));

    context->stackBottom = (uint64_t)page_allocator_request();
    context->stackTop = context->stackBottom + 0x1000;
    memset((void*)context->stackBottom, 0, 0x1000);
    context->state.stackPointer = context->stackTop;

    context->state.instructionPointer = (uint64_t)instructionPointer;
    context->state.codeSegment = codeSegment;
    context->state.stackSegment = stackSegment;
    context->state.flags = rFlags;

    VirtualAddressSpace* addressSpace = virtual_memory_create();

    virtual_memory_remap(addressSpace, (void*)context->stackBottom, (void*)context->stackBottom, 1);

    virtual_memory_remap_pages(addressSpace, &_kernelStart, &_kernelStart, ((uint64_t)&_kernelEnd - (uint64_t)&_kernelStart) / 0x1000 + 1, 0);

    context->state.cr3 = (uint64_t)addressSpace;

    return context;
}

void context_free(Context* context)
{
    virtual_memory_erase((VirtualAddressSpace*)context->state.cr3);

    page_allocator_unlock_page((void*)context->stackBottom);
    kfree(context);
}

void context_save(Context* context, const InterruptStackFrame* state)
{
    memcpy(&(context->state), state, sizeof(InterruptStackFrame));
}

void context_load(const Context* context, InterruptStackFrame* state)
{
    memcpy(state, &(context->state), sizeof(InterruptStackFrame));
}
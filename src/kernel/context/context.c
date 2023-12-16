#include "context.h"

#include "string/string.h"
#include "page_allocator/page_allocator.h"
#include "heap/heap.h"

TaskContext* context_new(void* instructionPointer, uint64_t codeSegment, uint64_t stackSegment, uint64_t rFlags)
{
    TaskContext* context = kmalloc(sizeof(TaskContext));
    memset(context, 0, sizeof(TaskContext));

    context->StackBottom = (uint64_t)page_allocator_request();
    context->StackTop = context->StackBottom + 0x1000;
    memset((void*)context->StackBottom, 0, 0x1000);

    context->State.StackPointer = context->StackTop;
    context->State.CR3 = (uint64_t)virtual_memory_create();

    context->State.InstructionPointer = (uint64_t)instructionPointer;
    context->State.CodeSegment = codeSegment;
    context->State.StackSegment = stackSegment;
    context->State.Flags = rFlags;

    virtual_memory_remap((VirtualAddressSpace*)context->State.CR3, (void*)context->StackBottom, (void*)context->StackBottom, 1);

    return context;
}

void context_free(TaskContext* context)
{
    virtual_memory_erase((VirtualAddressSpace*)context->State.CR3);

    page_allocator_unlock_page((void*)context->StackBottom);
    kfree(context);
}

void context_save(TaskContext* context, const InterruptStackFrame* state)
{
    memcpy(&(context->State), state, sizeof(InterruptStackFrame));
}

void context_load(const TaskContext* context, InterruptStackFrame* state)
{
    memcpy(state, &(context->State), sizeof(InterruptStackFrame));
}
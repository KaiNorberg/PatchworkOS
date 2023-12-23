#include "context.h"

#include "string/string.h"
#include "page_allocator/page_allocator.h"
#include "heap/heap.h"
#include "gdt/gdt.h"
#include "idt/idt.h"

Context* context_new(void* instructionPointer, void* stackPointer, uint64_t codeSegment, uint64_t stackSegment, uint64_t rFlags, PageDirectory* pageDirectory)
{
    Context* context = kmalloc(sizeof(Context));
    memset(context, 0, sizeof(Context));

    context->state.stackPointer = (uint64_t)stackPointer;
    context->state.instructionPointer = (uint64_t)instructionPointer;
    context->state.codeSegment = codeSegment;
    context->state.stackSegment = stackSegment;
    context->state.flags = rFlags;

    context->state.cr3 = (uint64_t)pageDirectory;

    return context;
}

void context_free(Context* context)
{
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
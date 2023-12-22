#include "context.h"

#include "string/string.h"
#include "page_allocator/page_allocator.h"
#include "heap/heap.h"
#include "gdt/gdt.h"
#include "idt/idt.h"

Context* context_new(void* instructionPointer, uint64_t codeSegment, uint64_t stackSegment, uint64_t rFlags)
{
    Context* context = kmalloc(sizeof(Context));
    memset(context, 0, sizeof(Context));

    context->stackBottom = (uint64_t)page_allocator_request();
    context->stackTop = context->stackBottom + 0x1000;
    memset((void*)context->stackBottom, 0, 0x1000);

    context->state.stackPointer = (uint64_t)USER_ADDRESS_SPACE_STACK_TOP_PAGE + 0x1000;
    context->state.instructionPointer = (uint64_t)instructionPointer;
    context->state.codeSegment = codeSegment;
    context->state.stackSegment = stackSegment;
    context->state.flags = rFlags;

    PageDirectory* pageDirectory = page_directory_create();

    page_directory_remap(pageDirectory, USER_ADDRESS_SPACE_STACK_TOP_PAGE, (void*)context->stackBottom, 1);

    context->state.cr3 = (uint64_t)pageDirectory;

    return context;
}

void context_free(Context* context)
{
    page_directory_erase((PageDirectory*)context->state.cr3);

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
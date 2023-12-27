#include "interrupt_frame.h"

#include "heap/heap.h"
#include "string/string.h"

InterruptFrame* interrupt_frame_new(void* instructionPointer, void* stackPointer, uint64_t codeSegment, uint64_t stackSegment, uint64_t rFlags, PageDirectory* pageDirectory)
{
    InterruptFrame* interruptFrame = kmalloc(sizeof(InterruptFrame));
    memset(interruptFrame, 0, sizeof(InterruptFrame));

    interruptFrame->stackPointer = (uint64_t)stackPointer;
    interruptFrame->instructionPointer = (uint64_t)instructionPointer;
    interruptFrame->codeSegment = codeSegment;
    interruptFrame->stackSegment = stackSegment;
    interruptFrame->flags = rFlags;

    interruptFrame->cr3 = (uint64_t)pageDirectory;

    return interruptFrame;
}

void interrupt_frame_free(InterruptFrame* interruptFrame)
{
    kfree(interruptFrame);
}

void interrupt_frame_copy(InterruptFrame* dest, InterruptFrame* src)
{
    memcpy(dest, src, sizeof(InterruptFrame));
}
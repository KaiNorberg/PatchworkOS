#include "interrupt_frame.h"

#include "heap/heap.h"
#include "string/string.h"
#include "process/process.h"

InterruptFrame* interrupt_frame_new(void* instructionPointer, void* stackPointer, uint64_t codeSegment, uint64_t stackSegment, PageDirectory* pageDirectory)
{
    InterruptFrame* interruptFrame = kmalloc(sizeof(InterruptFrame));
    memset(interruptFrame, 0, sizeof(InterruptFrame));

    interruptFrame->instructionPointer = (uint64_t)instructionPointer;
    interruptFrame->stackPointer = (uint64_t)stackPointer;
    interruptFrame->codeSegment = codeSegment;
    interruptFrame->stackSegment = stackSegment;
    interruptFrame->cr3 = (uint64_t)pageDirectory;

    interruptFrame->flags = 0x202;

    return interruptFrame;
}

void interrupt_frame_free(InterruptFrame* interruptFrame)
{
    kfree(interruptFrame);
}

InterruptFrame* interrupt_frame_duplicate(InterruptFrame const* src)
{    
    InterruptFrame* interruptFrame = kmalloc(sizeof(InterruptFrame));
    interrupt_frame_copy(interruptFrame, src);

    return interruptFrame;
}

void interrupt_frame_copy(InterruptFrame* dest, InterruptFrame const* src)
{
    memcpy(dest, src, sizeof(InterruptFrame));
}
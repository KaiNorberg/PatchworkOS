#include "interrupt_frame.h"

#include <libc/string.h>

#include "gdt/gdt.h"
#include "heap/heap.h"
#include "registers/registers.h"

InterruptFrame* interrupt_frame_new(void* instructionPointer, void* stackPointer)
{
    InterruptFrame* interruptFrame = kmalloc(sizeof(InterruptFrame));
    memset(interruptFrame, 0, sizeof(InterruptFrame));

    interruptFrame->instructionPointer = (uint64_t)instructionPointer;
    interruptFrame->stackPointer = (uint64_t)stackPointer;
    interruptFrame->codeSegment = GDT_USER_CODE | 3;
    interruptFrame->stackSegment = GDT_USER_DATA | 3;
    interruptFrame->flags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

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
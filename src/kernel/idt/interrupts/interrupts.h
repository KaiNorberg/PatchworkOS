#pragma once

#include <stdint.h>

typedef struct 
{
    uint64_t InstructionPointer;
    uint64_t CodeSegment;
    uint64_t Flags;
    uint64_t StackPointer;
    uint64_t StackSegment;
} InterruptStackFrame;

/////////////////////////////////
// Exception interrupt handlers.
/////////////////////////////////

__attribute__((interrupt)) void device_by_zero_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void none_maskable_interrupt_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void breakpoint_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void overflow_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void boundRange_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void invalid_opcode_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void device_not_detected_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void double_fault_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void invalid_tts_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void segment_not_present_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void stack_segment_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void general_protection_exception(InterruptStackFrame* frame);

__attribute__((interrupt)) void page_fault_exception(InterruptStackFrame* frame, uint64_t errorCode);

__attribute__((interrupt)) void floating_point_exception(InterruptStackFrame* frame);

/////////////////////////////////
// IRQ interrupt handlers.
/////////////////////////////////

__attribute__((interrupt)) void keyboard_interrupt(InterruptStackFrame* frame);

/*__attribute__((interrupt)) void syscall_interrupt(InterruptStackFrame* frame);*/
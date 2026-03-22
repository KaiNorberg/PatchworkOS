.code64

#include "kernel/cpu/interrupt.h"

.extern kmain
.extern thread_load

.global thread_jump
.type thread_jump, @function

.text

// rdi = thread_t*
thread_jump:
    cli

    // Allocate buffer for interrupt frame
    subq $INTERRUPT_FRAME_SIZE, %rsp

    // thread_load(rdi, rsp)
    movq %rsp, %rsi
    call thread_load

    INTERRUPT_FRAME_REGS_POP

    addq $16, %rsp
    iretq

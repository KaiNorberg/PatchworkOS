[bits 64]

%include "kernel/cpu/interrupt.inc"

extern kmain
extern thread_load

global thread_jump:function

section .text

; rdi = thread_t*
thread_jump:
    cli

    ; Allocate buffer for interrupt frame
    sub rsp, INTERRUPT_FRAME_SIZE

    ; thread_load(rdi, rsp)
    mov rsi, rsp
    call thread_load

    INTERRUPT_FRAME_REGS_POP

    add rsp, 16
    iretq

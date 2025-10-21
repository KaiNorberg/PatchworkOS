[bits 64]

%include "cpu/gdt.inc"
%include "cpu/regs.inc"
%include "cpu/interrupt.inc"

extern thread_get_interrupt_frame

section .text

;rdi = currently running thread
global loader_jump_to_user_space
loader_jump_to_user_space:
    cli

    ; thread_get_interrupt_frame(rdi, rsp)
    sub rsp, INTERRUPT_FRAME_SIZE
    mov rsi, rsp
    call thread_get_interrupt_frame

    INTERRUPT_FRAME_POP_AND_JUMP
    ud2 ; Should never be reached

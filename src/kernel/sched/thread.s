[bits 64]

%include "cpu/interrupt.inc"

extern kmain
extern thread_load

section .text

; rdi = thread_t*
global thread_jump
thread_jump:
    cli

    ; Allocate buffer for interrupt frame
    sub rsp, INTERRUPT_FRAME_SIZE

    ; thread_load(rdi, rsp)
    mov rsi, rsp
    call thread_load

    INTERRUPT_FRAME_POP_AND_JUMP
    ud2 ; Should never be reached

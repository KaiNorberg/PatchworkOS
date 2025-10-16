[bits 64]

%include "cpu/gdt.inc"
%include "cpu/regs.inc"
%include "cpu/trap.inc"

extern thread_get_trap_frame

section .text

;rdi = currently running thread
global loader_jump_to_user_space
loader_jump_to_user_space:
    ; thread_get_trap_frame(rdi, rsp)
    sub rsp, TRAP_FRAME_SIZE
    mov rsi, rsp
    call thread_get_trap_frame

    TRAP_FRAME_POP_AND_JUMP
    ud2 ; Should never be reached

[bits 64]

%include "cpu/trap.inc"

extern syscall_handler

section .text

ALIGN 4096
global syscall_entry
syscall_entry:
    swapgs
    mov [gs:0x8], rsp ; Save user stack to syscall ctx
    mov rsp, [gs:0x0] ; Load kernel stack from syscall ctx
    sti
    
    ; Create trap frame, let the non registers be full of garbage
    sub rsp, 7 * 8
    TRAP_FRAME_REGS_PUSH

    mov rbp, rsp
    mov rdi, rsp
    call syscall_handler

    TRAP_FRAME_REGS_POP
    add rsp, 7 * 8
    
    cli
    mov rsp, [gs:0x8] ; Load user stack from syscall ctx
    swapgs
    o64 sysret
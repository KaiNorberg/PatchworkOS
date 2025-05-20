
%include "cpu/trap.inc"
%include "kernel/syscalls.inc"

extern syscall_handler

section .text

global syscall_vector
syscall_vector:
    cmp rax, SYS_TOTAL_AMOUNT
    jae .not_available

    push qword 0 ; Push error code (none)
    push qword SYSCALL_VECTOR ; Push vector
    TRAP_FRAME_REGS_PUSH

    mov rbp, rsp
    mov rdi, rsp
    call syscall_handler

    TRAP_FRAME_REGS_POP
    add rsp, 16
    iretq
.not_available:
    mov rax, -1
    iretq

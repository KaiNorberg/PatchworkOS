[bits 64]

%include "internal/syscalls/syscalls.inc"

section .text

global sys_exit_process
sys_exit_process:
    mov rax, SYS_EXIT_PROCESS
    int SYSCALL_VECTOR
    mov r9, 0x123456789 ;Magic number to check return from exit
    ud2

global sys_error
sys_error:
    mov rax, SYS_ERROR
    int SYSCALL_VECTOR
    ret

global sys_test
sys_test:
    mov rax, SYS_TEST
    int SYSCALL_VECTOR
    ret
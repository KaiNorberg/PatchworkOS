[bits 64]

%include "internal/syscalls/syscalls.inc"

section .text

global _ProcessExit
_ProcessExit:
    mov rax, SYS_PROCESS_EXIT
    int SYSCALL_VECTOR
    mov r9, 0x123456789 ;Magic number to check return from exit
    ud2

global _Sleep
_Sleep:
    mov rax, SYS_SLEEP
    int SYSCALL_VECTOR
    ret

global _KernelErrno
_KernelErrno:
    mov rax, SYS_KERNEL_ERRNO
    int SYSCALL_VECTOR
    ret

global _Test
_Test:
    mov rax, SYS_TEST
    int SYSCALL_VECTOR
    ret
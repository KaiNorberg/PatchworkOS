%ifndef __EMBED__

%define SYSCALL_VECTOR 0x80

section .text

;rdi = selector
global _Syscall0
_Syscall0:
    mov rax, rdi
    int SYSCALL_VECTOR
    ret

;rdi = arg1
;rsi = selector
global _Syscall1
_Syscall1:
    mov rax, rsi
    int SYSCALL_VECTOR
    ret

;rdi = arg1
;rsi = arg2
;rdx = selector
global _Syscall2
_Syscall2:
    mov rax, rdx
    int SYSCALL_VECTOR
    ret

;rdi = arg1
;rsi = arg2
;rdx = arg3
;rcx = selector
global _Syscall3
_Syscall3:
    mov rax, rcx
    int SYSCALL_VECTOR
    ret

;rdi = arg1
;rsi = arg2
;rdx = arg4
;rcx = arg4
;r8 = selector
global _Syscall4
_Syscall4:
    mov rax, r8
    int SYSCALL_VECTOR
    ret

;rdi = arg1
;rsi = arg2
;rdx = arg4
;rcx = arg4
;r8 = arg5
;r9 = selector
global _Syscall5
_Syscall5:
    mov rax, r9
    int SYSCALL_VECTOR
    ret

%endif

%include "kernel/syscalls.inc"

extern _ErrnoFunc

%macro SYSTEM_CALL_ERROR_CHECK 0
    push rax
    push rbx

    mov rax, SYS_ERROR
    int 0x80
    push rax
    call _ErrnoFunc
    pop rbx
    mov [rax], rbx ; Set errno

    pop rbx
    pop rax
%endmacro

%macro SYSTEM_CALL 1
    mov rax, %1
    int 0x80
    cmp rax, qword -1
    jne .no_error
    SYSTEM_CALL_ERROR_CHECK
.no_error:
%endmacro

%macro SYSTEM_CALL_PTR 1
    mov rax, %1
    int 0x80
    test rax, rax
    jnz .no_error
    SYSTEM_CALL_ERROR_CHECK
.no_error:
%endmacro

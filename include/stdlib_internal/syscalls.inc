%define SYS_PROCESS_EXIT 0
%define SYS_THREAD_EXIT 1
%define SYS_SPAWN 2
%define SYS_SLEEP 3
%define SYS_ERROR 4
%define SYS_PID 5
%define SYS_TID 6
%define SYS_UPTIME 7
%define SYS_OPEN 8
%define SYS_CLOSE 9
%define SYS_READ 10
%define SYS_WRITE 11
%define SYS_SEEK 12
%define SYS_IOCTL 13
%define SYS_REALPATH 14
%define SYS_CHDIR 15
%define SYS_POLL 16
%define SYS_STAT 17
%define SYS_MMAP 18
%define SYS_MUNMAP 19
%define SYS_MPROTECT 20
%define SYS_FLUSH 21

%define SYS_TOTAL_AMOUNT 22

%ifndef __EMBED__

extern _ErrnoFunc

%macro SYSTEM_CALL_ERROR_CHECK 0
    push rax
    push rbx

    mov rax, SYS_ERROR
    int 0x80
    push rax
    call _ErrnoFunc
    pop rbx
    mov [rax], rbx

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

%endif

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
%define SYS_STAT 10
%define SYS_READ 11
%define SYS_WRITE 12
%define SYS_SEEK 13
%define SYS_IOCTL 14
%define SYS_REALPATH 15
%define SYS_CHDIR 16
%define SYS_POLL 17
%define SYS_MMAP 18
%define SYS_MUNMAP 19
%define SYS_MPROTECT 20

%define SYS_TOTAL_AMOUNT 21

%macro SYSTEM_CALL_ERROR_CHECK 0
extern _ErrnoFunc
jne .no_error
    push rax
    push rbx

    mov rax, SYS_ERROR
    int 0x80
    mov rbx, rax
    call _ErrnoFunc
    mov [rax], rbx

    pop rbx
    pop rax
.no_error:
%endmacro

%macro SYSTEM_CALL 1
    mov rax, %1
    int 0x80
    cmp rax, qword -1
    SYSTEM_CALL_ERROR_CHECK
%endmacro

%macro SYSTEM_CALL_PTR 1
    mov rax, %1
    int 0x80
    cmp rax, qword 0
    SYSTEM_CALL_ERROR_CHECK
%endmacro

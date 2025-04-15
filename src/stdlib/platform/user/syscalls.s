%include "kernel/syscalls.inc"

extern _ErrnoFunc

%macro SYSTEM_CALL_ERROR_CHECK 0
    push rax
    push rbx

    mov rax, SYS_LAST_ERROR
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

section .text

global _SyscallProcessExit
_SyscallProcessExit:
    SYSTEM_CALL SYS_PROCESS_EXIT
    ret

global _SyscallThreadExit
_SyscallThreadExit:
    SYSTEM_CALL SYS_THREAD_EXIT
    ret

global _SyscallSpawn
_SyscallSpawn:
    SYSTEM_CALL SYS_SPAWN
    ret

global _SyscallSleep
_SyscallSleep:
    SYSTEM_CALL SYS_SLEEP
    ret

global _SyscallUptime
_SyscallUptime:
    SYSTEM_CALL SYS_UPTIME
    ret

global _SyscallUnixEpoch
_SyscallUnixEpoch:
    SYSTEM_CALL SYS_UNIX_EPOCH
    ret

global _SyscallLastError
_SyscallError:
    SYSTEM_CALL SYS_LAST_ERROR
    ret

global _SyscallProcessId
_SyscallProcessId:
    SYSTEM_CALL SYS_PROCESS_ID
    ret

global _SyscallThreadId
_SyscallThreadId:
    SYSTEM_CALL SYS_THREAD_ID
    ret

global _SyscallOpen
_SyscallOpen:
    SYSTEM_CALL SYS_OPEN
    ret

global _SyscallOpen2
_SyscallOpen2:
    SYSTEM_CALL SYS_OPEN2
    ret

global _SyscallClose
_SyscallClose:
    SYSTEM_CALL SYS_CLOSE
    ret

global _SyscallRead
_SyscallRead:
    SYSTEM_CALL SYS_READ
    ret

global _SyscallWrite
_SyscallWrite:
    SYSTEM_CALL SYS_WRITE
    ret

global _SyscallSeek
_SyscallSeek:
    SYSTEM_CALL SYS_SEEK
    ret

global _SyscallIoctl
_SyscallIoctl:
    SYSTEM_CALL SYS_IOCTL
    ret

global _SyscallChdir
_SyscallChdir:
    SYSTEM_CALL SYS_CHDIR
    ret

global _SyscallPoll
_SyscallPoll:
    SYSTEM_CALL SYS_POLL
    ret

global _SyscallStat
_SyscallStat:
    SYSTEM_CALL SYS_STAT
    ret

global _SyscallVirtualAlloc
_SyscallVirtualAlloc:
    SYSTEM_CALL_PTR SYS_VIRTUAL_ALLOC
    ret

global _SyscallVirtualFree
_SyscallVirtualFree:
    SYSTEM_CALL SYS_VIRTUAL_FREE
    ret

global _SyscallVirtualProtect
_SyscallVirtualProtect:
    SYSTEM_CALL SYS_VIRTUAL_PROTECT
    ret

global _SyscallFlush
_SyscallFlush:
    SYSTEM_CALL SYS_FLUSH
    ret

global _SyscallDirList
_SyscallDirList:
    SYSTEM_CALL SYS_DIR_LIST
    ret

global _SyscallThreadCreate
_SyscallThreadCreate:
    SYSTEM_CALL SYS_THREAD_CREATE
    ret

global _SyscallYield
_SyscallYield:
    SYSTEM_CALL SYS_YIELD
    ret

global _SyscallOpenas
_SyscallOpenas:
    SYSTEM_CALL SYS_OPENAS
    ret

global _SyscallOpen2as
_SyscallOpen2as:
    SYSTEM_CALL SYS_OPEN2AS
    ret

global _SyscallDup
_SyscallDup:
    SYSTEM_CALL SYS_DUP
    ret

global _SyscallDup2
_SyscallDup2:
    SYSTEM_CALL SYS_DUP2
    ret

global _SyscallFutex
_SyscallFutex:
    SYSTEM_CALL SYS_FUTEX
    ret

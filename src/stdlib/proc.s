%ifndef __EMBED__

%include "stdlib_internal/syscalls.inc"

section .text

global sleep
sleep:
    SYSTEM_CALL SYS_SLEEP
    ret

global munmap
munmap:
    SYSTEM_CALL SYS_MUNMAP
    ret

global mprotect
mprotect:
    SYSTEM_CALL SYS_MPROTECT
    ret

global mmap
mmap:
    SYSTEM_CALL_PTR SYS_MMAP
    ret

global getpid
getpid:
    SYSTEM_CALL SYS_PID
    ret

global uptime
uptime:
    SYSTEM_CALL SYS_UPTIME
    ret

global spawn
spawn:
    SYSTEM_CALL SYS_SPAWN
    ret

%endif

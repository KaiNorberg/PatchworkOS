%ifndef __EMBED__

%include "internal/syscall.inc"

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

global gettid
gettid:
    SYSTEM_CALL SYS_TID
    ret

global uptime
uptime:
    SYSTEM_CALL SYS_UPTIME
    ret

global spawn
spawn:
    SYSTEM_CALL SYS_SPAWN
    ret

global split
split:
    SYSTEM_CALL SYS_SPLIT
    ret

global thread_exit
thread_exit:
    SYSTEM_CALL SYS_THREAD_EXIT
    ret

global yield
yield:
    SYSTEM_CALL SYS_YIELD
    ret

%endif

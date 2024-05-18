%include "libs/std/internal/syscalls.inc"

global getpid
getpid:
    SYSTEM_CALL SYS_PID
    ret
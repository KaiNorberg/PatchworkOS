%include "libs/std/internal/syscalls.inc"

global uptime
uptime:
    SYSTEM_CALL SYS_UPTIME
    ret
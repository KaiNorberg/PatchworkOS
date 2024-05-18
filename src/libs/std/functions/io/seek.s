%include "libs/std/internal/syscalls.inc"

global seek
seek:
    SYSTEM_CALL SYS_SEEK
    ret
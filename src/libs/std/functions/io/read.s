%include "libs/std/internal/syscalls.inc"

global read
read:
    SYSTEM_CALL SYS_READ
    ret
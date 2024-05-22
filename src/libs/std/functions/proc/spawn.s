%include "libs/std/internal/syscalls.inc"

global spawn
spawn:
    SYSTEM_CALL SYS_SPAWN
    ret
%include "libs/std/internal/syscalls.inc"

global write
write:
    SYSTEM_CALL SYS_WRITE
    ret
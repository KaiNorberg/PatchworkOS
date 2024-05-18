%include "libs/std/internal/syscalls.inc"

global mprotect
mprotect:
    SYSTEM_CALL SYS_MPROTECT
    ret
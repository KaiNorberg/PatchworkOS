%include "libs/std/internal/syscalls.inc"

global accept
accept:
    SYSTEM_CALL SYS_ACCEPT
    ret
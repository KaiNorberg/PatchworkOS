%include "libs/std/internal/syscalls.inc"

global flush
flush:
    SYSTEM_CALL SYS_FLUSH
    ret
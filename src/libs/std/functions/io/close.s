%include "libs/std/internal/syscalls.inc"

global close
close:
    SYSTEM_CALL SYS_CLOSE
    ret
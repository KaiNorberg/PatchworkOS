%include "libs/std/internal/syscalls.inc"

global chdir
chdir:
    SYSTEM_CALL SYS_CHDIR
    ret
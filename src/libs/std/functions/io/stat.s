%include "libs/std/internal/syscalls.inc"

global stat
stat:
    SYSTEM_CALL SYS_STAT
    ret
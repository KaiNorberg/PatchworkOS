%include "libs/std/internal/syscalls.inc"

global announce
announce:
    SYSTEM_CALL SYS_ANNOUNCE
    ret
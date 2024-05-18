%include "libs/std/internal/syscalls.inc"

global realpath
realpath:
    SYSTEM_CALL SYS_REALPATH
    ret
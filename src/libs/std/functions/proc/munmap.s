%include "libs/std/internal/syscalls.inc"

global munmap
munmap:
    SYSTEM_CALL SYS_MUNMAP
    ret
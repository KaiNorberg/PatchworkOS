%include "libs/std/internal/syscalls.inc"

global mmap
mmap:
    SYSTEM_CALL_PTR SYS_MMAP
    ret
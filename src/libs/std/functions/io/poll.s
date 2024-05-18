%include "libs/std/internal/syscalls.inc"

global poll
poll:
    SYSTEM_CALL SYS_POLL
    ret
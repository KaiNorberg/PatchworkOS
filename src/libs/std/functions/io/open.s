%include "libs/std/internal/syscalls.inc"

global open
open:
    SYSTEM_CALL SYS_OPEN
    ret
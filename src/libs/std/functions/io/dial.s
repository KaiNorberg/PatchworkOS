%include "libs/std/internal/syscalls.inc"

global dial
dial:
    SYSTEM_CALL SYS_DIAL
    ret
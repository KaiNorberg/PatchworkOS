%include "libs/std/internal/syscalls.inc"

global sleep
sleep:
    SYSTEM_CALL SYS_SLEEP
    ret
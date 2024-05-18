%include "libs/std/internal/syscalls.inc"

global exit
exit:
    SYSTEM_CALL SYS_PROCESS_EXIT
    ud2

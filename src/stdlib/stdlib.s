%ifndef __EMBED__

%include "stdlib_internal/syscalls.inc"

global exit
exit:
    SYSTEM_CALL SYS_PROCESS_EXIT
    ud2

%endif

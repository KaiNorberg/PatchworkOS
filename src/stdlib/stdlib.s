%ifndef __EMBED__

%include "internal/syscall.inc"

global exit
exit:
    SYSTEM_CALL SYS_PROCESS_EXIT
    ud2

%endif

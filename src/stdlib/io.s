%ifndef __EMBED__

%include "internal/syscall.inc"

section .text

global chdir
chdir:
    SYSTEM_CALL SYS_CHDIR
    ret

global close
close:
    SYSTEM_CALL SYS_CLOSE
    ret

global flush
flush:
    SYSTEM_CALL SYS_FLUSH
    ret

global ioctl
ioctl:
    SYSTEM_CALL SYS_IOCTL
    ret

global open
open:
    SYSTEM_CALL SYS_OPEN
    ret

global poll
poll:
    SYSTEM_CALL SYS_POLL
    ret

global read
read:
    SYSTEM_CALL SYS_READ
    ret

global realpath
realpath:
    SYSTEM_CALL SYS_REALPATH
    ret

global seek
seek:
    SYSTEM_CALL SYS_SEEK
    ret

global stat
stat:
    SYSTEM_CALL SYS_STAT
    ret

global write
write:
    SYSTEM_CALL SYS_WRITE
    ret

%endif

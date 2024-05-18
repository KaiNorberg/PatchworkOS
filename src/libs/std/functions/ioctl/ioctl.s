%include "libs/std/internal/syscalls.inc"

global ioctl
ioctl:
    SYSTEM_CALL SYS_IOCTL
    ret
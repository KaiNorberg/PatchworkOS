%include "platform/user/syscall.inc"

section .text

global _PlatformChdir
_PlatformChdir:
    SYSTEM_CALL SYS_CHDIR
    ret

global _PlatformClose
_PlatformClose:
    SYSTEM_CALL SYS_CLOSE
    ret

global _PlatformFlush
_PlatformFlush:
    SYSTEM_CALL SYS_FLUSH
    ret

global _PlatformIoctl
_PlatformIoctl:
    SYSTEM_CALL SYS_IOCTL
    ret

global _PlatformOpen
_PlatformOpen:
    SYSTEM_CALL SYS_OPEN
    ret

global _PlatformPoll
_PlatformPoll:
    SYSTEM_CALL SYS_POLL
    ret

global _PlatformRead
_PlatformRead:
    SYSTEM_CALL SYS_READ
    ret

global _PlatformRealpath
_PlatformRealpath:
    SYSTEM_CALL SYS_REALPATH
    ret

global _PlatformSeek
_PlatformSeek:
    SYSTEM_CALL SYS_SEEK
    ret

global _PlatformStat
_PlatformStat:
    SYSTEM_CALL SYS_STAT
    ret

global _PlatformWrite
_PlatformWrite:
    SYSTEM_CALL SYS_WRITE
    ret

global _PlatformListdir
_PlatformListdir:
    SYSTEM_CALL SYS_LISTDIR
    ret

global _PlatformPipe
_PlatformPipe:
    SYSTEM_CALL SYS_PIPE
    ret

global _PlatformSleep
_PlatformSleep:
    SYSTEM_CALL SYS_SLEEP
    ret

global _PlatformGetpid
_PlatformGetpid:
    SYSTEM_CALL SYS_PID
    ret

global _PlatformGettid
_PlatformGettid:
    SYSTEM_CALL SYS_TID
    ret

global _PlatformUptime
_PlatformUptime:
    SYSTEM_CALL SYS_UPTIME
    ret

global _PlatformTime
_PlatformTime:
    SYSTEM_CALL SYS_TIME
    ret

global _PlatformSpawn
_PlatformSpawn:
    SYSTEM_CALL SYS_SPAWN
    ret

global _PlatformSplit
_PlatformSplit:
    SYSTEM_CALL SYS_SPLIT
    ret

global _PlatformThreadExit
_PlatformThreadExit:
    SYSTEM_CALL SYS_THREAD_EXIT
    ret

global _PlatformYield
_PlatformYield:
    SYSTEM_CALL SYS_YIELD
    ret

global _PlatformMmap
_PlatformMmap:
    SYSTEM_CALL_PTR SYS_MMAP
    ret

global _PlatformMunmap
_PlatformMunmap:
    SYSTEM_CALL SYS_MUNMAP
    ret

global _PlatformMprotect
_PlatformMprotect:
    SYSTEM_CALL SYS_MPROTECT
    ret

global _PlatformExit
_PlatformExit:
    SYSTEM_CALL SYS_PROCESS_EXIT
    ud2
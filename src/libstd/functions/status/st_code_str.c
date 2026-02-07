#include <sys/status.h>

const char* st_code_str(st_code_t code)
{
    switch (code)
    {
    case ST_CODE_NONE:
        return "no specific code";

    // Generic
    case ST_CODE_UNKNOWN:
        return "unknown error";
    case ST_CODE_INVAL:
        return "invalid argument";
    case ST_CODE_OVERFLOW:
        return "buffer overflow";
    case ST_CODE_TOOBIG:
        return "value too big";
    case ST_CODE_TIMEOUT:
        return "operation timed out";
    case ST_CODE_CANCELLED:
        return "operation cancelled";
    case ST_CODE_NOT_CANCELLABLE:
        return "operation cannot be cancelled";
    case ST_CODE_IMPL:
        return "implementation error";
    case ST_CODE_AGAIN:
        return "resource temporarily unavailable";
    case ST_CODE_INTR:
        return "interrupted system call";
    case ST_CODE_INVALFLAG:
        return "invalid flag";
    case ST_CODE_CHANGED:
        return "state changed";
    case ST_CODE_FULL:
        return "buffer full";
    case ST_CODE_MORE:
        return "more data available";
    case ST_CODE_ARGC:
        return "invalid argument count";
    case ST_CODE_INVALCTL:
        return "invalid control command";
    case ST_CODE_TOCTOU:
        return "time-of-check to time-of-use race condition";
    case ST_CODE_TEST_FAIL:
        return "test failure";
    case ST_CODE_PENDING:
        return "operation is pending";
    case ST_CODE_COMPLETE:
        return "operation has been completed";

    // Memory
    case ST_CODE_NOMEM:
        return "out of memory";
    case ST_CODE_NOSPACE:
        return "no space left";
    case ST_CODE_FAULT:
        return "bad address";
    case ST_CODE_ALIGN:
        return "alignment error";
    case ST_CODE_MAPPED:
        return "already mapped";
    case ST_CODE_UNMAPPED:
        return "not mapped";
    case ST_CODE_PINNED:
        return "page pinned";
    case ST_CODE_SHARED_LIMIT:
        return "shared memory limit reached";
    case ST_CODE_IN_STACK:
        return "address in stack";

    // I/O
    case ST_CODE_PATHTOOLONG:
        return "path too long";
    case ST_CODE_NAMETOOLONG:
        return "name too long";
    case ST_CODE_INVALCHAR:
        return "invalid character";
    case ST_CODE_FD_OVERFLOW:
        return "file descriptor overflow";
    case ST_CODE_MFILE:
        return "too many open files";
    case ST_CODE_BADFD:
        return "bad file descriptor";
    case ST_CODE_NOENT:
        return "no such file or directory";
    case ST_CODE_NOTDIR:
        return "not a directory";
    case ST_CODE_ISDIR:
        return "is a directory";
    case ST_CODE_BUSY:
        return "device or resource busy";
    case ST_CODE_EXIST:
        return "file exists";
    case ST_CODE_XDEV:
        return "cross-device link";
    case ST_CODE_NOTEMPTY:
        return "directory not empty";
    case ST_CODE_IO:
        return "i/o error";
    case ST_CODE_SHADOW_LIMIT:
        return "shadow mount limit reached";
    case ST_CODE_LOOP:
        return "too many symbolic links";
    case ST_CODE_NOFS:
        return "no filesystem found";
    case ST_CODE_NEGATIVE:
        return "path component does not exist";
    case ST_CODE_SPIPE:
        return "invalid seek";
    case ST_CODE_ADDRINUSE:
        return "address already in use";
    case ST_CODE_EXPECT_FILE:
        return "operation expected to be provided a file";

    // Access
    case ST_CODE_ACCESS:
        return "permission denied";
    case ST_CODE_PERM:
        return "operation not permitted";
    case ST_CODE_NOGROUP:
        return "not within a group";
    case ST_CODE_INVAL_KEY:
        return "invalid key";

    // Resource
    case ST_CODE_NOT_INIT:
        return "resource is not initialized";
    case ST_CODE_ALREADY_INIT:
        return "resource is already initialized";
    case ST_CODE_ACQUIRED:
        return "resource is already acquired";

    // Process
    case ST_CODE_DYING:
        return "process is dying";
    case ST_CODE_DEADLOCK:
        return "deadlock detected";

    // Device
    case ST_CODE_NODEV:
        return "no such device";
    case ST_CODE_NOTTY:
        return "inappropriate ioctl for device";
    case ST_CODE_RAND:
        return "random number generator error";
    case ST_CODE_MCLOCK:
        return "too many clock sources";
    case ST_CODE_MTIMER:
        return "too many timer sources";

    // Format
    case ST_CODE_INVALELF:
        return "invalid elf executable";
    case ST_CODE_ILSEQ:
        return "invalid byte sequence";
    case ST_CODE_NO_ACPI_TABLE:
        return "unable to locate acpi table";
    case ST_CODE_INVAL_ACPI_TABLE:
        return "invalid acpi table";
    case ST_CODE_NO_BOOT_INFO:
        return "bootloader did not provide needed info";

    // System
    case ST_CODE_MJ_OVERFLOW:
        return "major number overflow";
    case ST_CODE_MJ_NOSYS:
        return "major number not found";
    case ST_CODE_MJ_INVAL:
        return "invalid major number";

    default:
        return "unknown";
    }
}
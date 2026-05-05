#include <libc/status.h>

const char* status_code_str(status_code_t code)
{
    switch (code)
    {
    case STATUS_CODE_NONE:
        return "no specific code";

    // Generic
    case STATUS_CODE_UNKNOWN:
        return "unknown error";
    case STATUS_CODE_INVAL:
        return "invalid argument";
    case STATUS_CODE_OVERFLOW:
        return "buffer overflow";
    case STATUS_CODE_TOOBIG:
        return "value too big";
    case STATUS_CODE_TIMEOUT:
        return "operation timed out";
    case STATUS_CODE_CANCELLED:
        return "operation cancelled";
    case STATUS_CODE_NOT_CANCELLABLE:
        return "operation cannot be cancelled";
    case STATUS_CODE_IMPL:
        return "implementation error";
    case STATUS_CODE_AGAIN:
        return "resource temporarily unavailable";
    case STATUS_CODE_INTR:
        return "interrupted system call";
    case STATUS_CODE_INVALFLAG:
        return "invalid flag";
    case STATUS_CODE_CHANGED:
        return "state changed";
    case STATUS_CODE_FULL:
        return "buffer full";
    case STATUS_CODE_EOF:
        return "end of file";
    case STATUS_CODE_ARGC:
        return "invalid argument count";
    case STATUS_CODE_TOCTOU:
        return "time-of-check to time-of-use race condition";
    case STATUS_CODE_TEST_FAIL:
        return "test failure";
    case STATUS_CODE_PENDING:
        return "operation is pending";
    case STATUS_CODE_COMPLETE:
        return "operation has been completed";
    case STATUS_CODE_DEFERRED:
        return "operation has been deferred";

    // Memory
    case STATUS_CODE_NOMEM:
        return "out of memory";
    case STATUS_CODE_NOSPACE:
        return "no space left";
    case STATUS_CODE_FAULT:
        return "bad address";
    case STATUS_CODE_ALIGN:
        return "alignment error";
    case STATUS_CODE_MAPPED:
        return "already mapped";
    case STATUS_CODE_UNMAPPED:
        return "not mapped";
    case STATUS_CODE_PINNED:
        return "page pinned";
    case STATUS_CODE_SHARED_LIMIT:
        return "shared memory limit reached";
    case STATUS_CODE_IN_STACK:
        return "address in stack";

    // I/O
    case STATUS_CODE_PATHTOOLONG:
        return "path too long";
    case STATUS_CODE_NAMETOOLONG:
        return "name too long";
    case STATUS_CODE_INVALCHAR:
        return "invalid character";
    case STATUS_CODE_FD_OVERFLOW:
        return "file descriptor overflow";
    case STATUS_CODE_MFILE:
        return "too many open files";
    case STATUS_CODE_BADFD:
        return "bad file descriptor";
    case STATUS_CODE_NOENT:
        return "no such file or directory";
    case STATUS_CODE_NOTDIR:
        return "not a directory";
    case STATUS_CODE_ISDIR:
        return "is a directory";
    case STATUS_CODE_BUSY:
        return "device or resource busy";
    case STATUS_CODE_EXIST:
        return "file exists";
    case STATUS_CODE_XDEV:
        return "cross-device link";
    case STATUS_CODE_NOTEMPTY:
        return "directory not empty";
    case STATUS_CODE_IO:
        return "i/o error";
    case STATUS_CODE_SHADOW_LIMIT:
        return "shadow mount limit reached";
    case STATUS_CODE_LOOP:
        return "too many symbolic links";
    case STATUS_CODE_NOFS:
        return "no filesystem found";
    case STATUS_CODE_NEGATIVE:
        return "path component does not exist";
    case STATUS_CODE_SPIPE:
        return "invalid seek";
    case STATUS_CODE_ADDRINUSE:
        return "address already in use";
    case STATUS_CODE_EXPECT_FILE:
        return "operation expected to be provided a file";
    case STATUS_CODE_INVALCTL:
        return "invalid i/o control command";
    case STATUS_CODE_NOSUPPORT:
        return "operation not supported";

    // Access
    case STATUS_CODE_ACCESS:
        return "permission denied";
    case STATUS_CODE_PERM:
        return "operation not permitted";
    case STATUS_CODE_NOGROUP:
        return "not within a group";
    case STATUS_CODE_INVAL_KEY:
        return "invalid key";
    case STATUS_CODE_DOTDOT:
        return "attempt to access parent directory when not allowed";

    // Resource
    case STATUS_CODE_NOT_INIT:
        return "resource is not initialized";
    case STATUS_CODE_ALREADY_INIT:
        return "resource is already initialized";
    case STATUS_CODE_ACQUIRED:
        return "resource is already acquired";

    // Process
    case STATUS_CODE_DYING:
        return "process is dying";
    case STATUS_CODE_DEADLOCK:
        return "deadlock detected";
    case STATUS_CODE_RUNNING:
        return "process is already running";
    case STATUS_CODE_NOT_RUNNING:
        return "process is not running";

    // Device
    case STATUS_CODE_NODEV:
        return "no such device";
    case STATUS_CODE_NOTTY:
        return "inappropriate ioctl for device";
    case STATUS_CODE_RAND:
        return "random number generator error";
    case STATUS_CODE_MCLOCK:
        return "too many clock sources";
    case STATUS_CODE_MTIMER:
        return "too many timer sources";

    // Format
    case STATUS_CODE_INVALELF:
        return "invalid elf executable";
    case STATUS_CODE_ILSEQ:
        return "invalid byte sequence";
    case STATUS_CODE_NO_ACPI_TABLE:
        return "unable to locate acpi table";
    case STATUS_CODE_INVAL_ACPI_TABLE:
        return "invalid acpi table";
    case STATUS_CODE_NO_BOOT_INFO:
        return "bootloader did not provide needed info";
    case STATUS_CODE_NOINTERP:
        return "unable to locate elf interpreter";
    case STATUS_CODE_NOVERSION:
        return "invalid version";
    case STATUS_CODE_INVALSCON:
        return "invalid scon";

    // System
    case STATUS_CODE_MJ_OVERFLOW:
        return "major number overflow";
    case STATUS_CODE_MJ_NOSYS:
        return "major number not found";
    case STATUS_CODE_MJ_INVAL:
        return "invalid major number";

    default:
        return "unknown";
    }
}
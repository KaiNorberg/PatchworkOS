#ifndef _SYS_STATUS_H
#define _SYS_STATUS_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "../_libc/uint32_t.h"

/**
 * @brief System status codes.
 * @defgroup libc_status Status
 * @ingroup libc
 *
 * The status system is used to report errors and informational messages from various functions and
 * subsystems.
 *
 * ## Errors
 *
 * Using this system allows for more expressive error reporting, as you can know not just what happened, but also where
 * it happened and the "kind" of error that occurred.
 *
 * ## Information.
 *
 * The status value allows information to be passed even if a operation completed successfully.
 *
 * For example, if a read is performed and the end of the file is reached, an informational `STATUS_CODE_EOF` status
 * value will be returned.
 *
 * Using the `STATUS_CODE_EOF` status can in some cases allow us to avoid an entire additional read operation just to
 * check if there is more data available.
 *
 * @note There are no "warning" or similar status values. The reasoning for this decision is that it is often not
 * possible to distinguish between warnings and errors in a meaningful way, which leads to confusion. In practice, all a
 * system needs to know is "Can I use the result of this operation?".
 *
 * ## Format
 *
 * A status is made up of a severity, source, kind and detail values. The kind and detail value are often combined into
 * a "code" value. Included is a table describing the bit format of a status value:
 *
 * | Bit(s) | Description  |
 * | :----- | :----------- |
 * | 31     | Severity bit |
 * | 24-30  | Source       |
 * | 16-23  | Reserved     |
 * | 8-15   | Kind         |
 * | 0-7    | Detail       |
 *
 * @note For convenience, the standard libraries `printf()` implementation provides the `%Y` specifier for easily
 * formatting status values.
 *
 * @{
 */

/**
 * @brief Status value.
 * @struct status_t
 */
typedef uint32_t status_t;

/**
 * @brief Status severity.
 * @enum status_sev_t
 *
 * Specifies the severity of a status.
 */
typedef enum
{
    ST_SEV_INFO = 0, ///< Success/Informational.
    ST_SEV_ERR = 1,  ///< Error.
} status_sev_t;

/**
 * @brief Status source.
 * @enum status_src_t
 *
 * Specifies they layer of an operation or subsystem, that generated the status.
 *
 * @note This is not used to group status values, a `STATUS_CODE_NOMEM` does not necessarily have a `STATUS_CODE_MEM`
 * source.
 */
typedef enum
{
    STATUS_SRC_NONE,    ///< No specific source.
    STATUS_SRC_IO,      ///< Input/Output.
    STATUS_SRC_MEM,     ///< Memory management, primarily the Physical Memory Manager.
    STATUS_SRC_MMU,     ///< Memory Management Unit, used by subsystems related to virtual memory.
    STATUS_SRC_SIMD,    ///< SIMD operations.
    STATUS_SRC_SCHED,   ///< Scheduler.
    STATUS_SRC_INT,     ///< Interrupts.
    STATUS_SRC_SYNC,    ///< Synchronization primitives.
    STATUS_SRC_DRIVER,  ///< Device drivers.
    STATUS_SRC_FS,      ///< Filesystem.
    STATUS_SRC_VFS,     ///< Virtual Filesystem.
    STATUS_SRC_IPC,     ///< Inter-Process Communication.
    STATUS_SRC_LIBSTD,  ///< Userspace Standard Library.
    STATUS_SRC_USER,    ///< Userspace Program.
    STATUS_SRC_PROC,    ///< Process Subsystem.
    STATUS_SRC_MODULE,  ///< Module Loader.
    STATUS_SRC_PORT,    ///< CPU Port I/O.
    STATUS_SRC_SYSCALL, ///< Syscall.
    STATUS_SRC_ACPI,    ///< ACPI.
    STATUS_SRC_TEST,    ///< Testing.
    STATUS_SRC_PROTO,   ///< Networking Protocol.
    STATUS_SRC_DYNLINK, ///< Dynamic Linker.
    STATUS_SRC_MAX,     ///< Maximum source value.
} status_src_t;

/**
 * @brief Status kind.
 * @enum status_kind_t
 *
 * Specifies the category of the status code.
 */
typedef enum
{
    STATUS_KIND_NONE,     ///< No specific kind.
    STATUS_KIND_GENERIC,  ///< Generic errors.
    STATUS_KIND_MEM,      ///< Memory errors.
    STATUS_KIND_IO,       ///< I/O errors.
    STATUS_KIND_ACCESS,   ///< Access/Permission errors.
    STATUS_KIND_RESOURCE, ///< Resource errors.
    STATUS_KIND_PROC,     ///< Process errors.
    STATUS_KIND_DEV,      ///< Device errors.
    STATUS_KIND_FMT,      ///< Format errors.
    STATUS_KIND_SYS,      ///< System errors.
    STATUS_KIND_MAX,      ///< Maximum kind value.
} status_kind_t;

/**
 * @brief Status code.
 * @enum status_code_t
 *
 * Specifies the specific error or status condition.
 */
typedef enum
{
    STATUS_CODE_NONE = 0, ///< No specific code.

    STATUS_CODE_UNKNOWN = (STATUS_KIND_GENERIC << 8) | 1, ///< Unknown error.
    STATUS_CODE_INVAL,                                    ///< Invalid argument.
    STATUS_CODE_OVERFLOW,                                 ///< Buffer overflow.
    STATUS_CODE_TOOBIG,                                   ///< Value too big.
    STATUS_CODE_TIMEOUT,                                  ///< Operation timed out.
    STATUS_CODE_CANCELLED,                                ///< Operation cancelled.
    STATUS_CODE_NOT_CANCELLABLE,                          ///< Operation cannot be cancelled.
    STATUS_CODE_IMPL,                                     ///< Implementation error.
    STATUS_CODE_AGAIN,                                    ///< Resource temporarily unavailable.
    STATUS_CODE_INTR,                                     ///< Interrupted system call.
    STATUS_CODE_INVALFLAG,                                ///< Invalid flag.
    STATUS_CODE_CHANGED,                                  ///< State changed.
    STATUS_CODE_FULL,                                     ///< Buffer full.
    STATUS_CODE_EOF,                                      ///< End of file.
    STATUS_CODE_ARGC,                                     ///< Invalid argument count.
    STATUS_CODE_TOCTOU,                                   ///< Time-of-check to time-of-use race condition.
    STATUS_CODE_TEST_FAIL,                                ///< Test failure.
    STATUS_CODE_PENDING,                                  ///< Operation is pending.
    STATUS_CODE_COMPLETE,                                 ///< Operation has been completed.
    STATUS_CODE_DEFERRED,                                 ///< Operation has been deferred.

    STATUS_CODE_NOMEM = (STATUS_KIND_MEM << 8) | 1, ///< Out of memory.
    STATUS_CODE_NOSPACE,                            ///< No space left.
    STATUS_CODE_FAULT,                              ///< Bad address.
    STATUS_CODE_ALIGN,                              ///< Alignment error.
    STATUS_CODE_MAPPED,                             ///< Already mapped.
    STATUS_CODE_UNMAPPED,                           ///< Not mapped.
    STATUS_CODE_PINNED,                             ///< Page pinned.
    STATUS_CODE_SHARED_LIMIT,                       ///< Shared memory limit reached.
    STATUS_CODE_IN_STACK,                           ///< Address in stack.

    STATUS_CODE_PATHTOOLONG = (STATUS_KIND_IO << 8) | 1, ///< Path too long.
    STATUS_CODE_NAMETOOLONG,                             ///< Name too long.
    STATUS_CODE_INVALCHAR,                               ///< Invalid character.
    STATUS_CODE_FD_OVERFLOW,                             ///< File descriptor is over the maximum value.
    STATUS_CODE_MFILE,                                   ///< Too many file descriptors open.
    STATUS_CODE_BADFD,                                   ///< File descriptor is not open.
    STATUS_CODE_NOENT,                                   ///< No such file or directory.
    STATUS_CODE_NOTDIR,                                  ///< Not a directory.
    STATUS_CODE_ISDIR,                                   ///< Is a directory.
    STATUS_CODE_BUSY,                                    ///< Device or resource busy.
    STATUS_CODE_EXIST,                                   ///< File exists.
    STATUS_CODE_XDEV,                                    ///< Cross-device link.
    STATUS_CODE_NOTEMPTY,                                ///< Directory not empty.
    STATUS_CODE_IO,                                      ///< I/O error.
    STATUS_CODE_SHADOW_LIMIT,                            ///< Maximum shadow mount depth reached.
    STATUS_CODE_LOOP,                                    ///< Too many levels of symbolic links.
    STATUS_CODE_NOFS,                                    ///< No filesystem found.
    STATUS_CODE_NEGATIVE,                                ///< Path component does not exist.
    STATUS_CODE_SPIPE,                                   ///< Invalid seek.
    STATUS_CODE_ADDRINUSE,                               ///< Address already in use.
    STATUS_CODE_EXPECT_FILE,                             ///< Operation expected to be provided a file.
    STATUS_CODE_INVALCTL,                                ///< Invalid I/O control command.
    STATUS_CODE_NOSUPPORT,                               ///< Operation not supported.

    STATUS_CODE_ACCESS = (STATUS_KIND_ACCESS << 8) | 1, ///< Permission denied.
    STATUS_CODE_PERM,                                   ///< Operation not permitted.
    STATUS_CODE_NOGROUP,                                ///< Not within a group.
    STATUS_CODE_INVAL_KEY,                              ///< Invalid key.
    STATUS_CODE_DOTDOT,                                 ///< Attempt to access parent directory when not allowed.

    STATUS_CODE_NOT_INIT = (STATUS_KIND_RESOURCE << 8) | 1, ///< Resource is not initialized.
    STATUS_CODE_ALREADY_INIT,                               ///< Resource is already initialized.
    STATUS_CODE_ACQUIRED,                                   ///< Resource is already acquired.

    STATUS_CODE_DYING = (STATUS_KIND_PROC << 8) | 1, ///< Process is dying.
    STATUS_CODE_DEADLOCK,                            ///< Deadlock detected.
    STATUS_CODE_RUNNING,                             ///< Process is already running.
    STATUS_CODE_NOT_RUNNING,                         ///< Process is not running.

    STATUS_CODE_NODEV = (STATUS_KIND_DEV << 8) | 1, ///< No such device.
    STATUS_CODE_NOTTY,                              ///< Inappropriate ioctl for device.
    STATUS_CODE_RAND,                               ///< Hardware random number generator error.
    STATUS_CODE_MCLOCK,                             ///< Too many clock sources.
    STATUS_CODE_MTIMER,                             ///< To many timer sources.

    STATUS_CODE_INVALELF = (STATUS_KIND_FMT << 8) | 1, ///< Invalid ELF executable.
    STATUS_CODE_ILSEQ,                                 ///< Invalid byte sequence.
    STATUS_CODE_NO_ACPI_TABLE,                         ///< Unable to locate ACPI table.
    STATUS_CODE_INVAL_ACPI_TABLE,                      ///< Invalid ACPI table.
    STATUS_CODE_NO_BOOT_INFO,                          ///< Bootloader did not provide needed info.
    STATUS_CODE_NOINTERP,                              ///< Unable to locate ELF interpreter.
    STATUS_CODE_NOVERSION,                             ///< Invalid version.
    STATUS_CODE_INVALSCON,                             ///< Invalid SCON.

    STATUS_CODE_MJ_OVERFLOW = (STATUS_KIND_SYS << 8) | 1, ///< Major number overflow.
    STATUS_CODE_MJ_NOSYS,                                 ///< Major number not found.
    STATUS_CODE_MJ_INVAL,                                 ///< Invalid major number.
} status_code_t;

/**
 * @brief Create a status value.
 *
 * @param _severity The severity of the status.
 * @param _source The source of the status.
 * @param _code The specific code of the status.
 * @return The constructed status value.
 */
#define STATUS(_severity, _source, _code) \
    ((status_t)(((uint32_t)(_severity) & 0x1) << 31) | (((uint32_t)(_source) & 0x7F) << 24) | \
        ((uint32_t)(_code) & 0xFFFF))

/**
 * @brief Extract the severity from a status value.
 *
 * @param _status The status value.
 * @return The severity.
 */
#define ST_SEV(_status) (((_status) >> 31) & 0x1)

/**
 * @brief Extract the source from a status value.
 *
 * @param _status The status value.
 * @return The source.
 */
#define STATUS_SRC(_status) (((_status) >> 24) & 0x7F)

/**
 * @brief Extract the code from a status value.
 *
 * @param _status The status value.
 * @return The code.
 */
#define STATUS_CODE(_status) ((_status) & 0xFFFF)

/**
 * @brief Extract the kind from a status value.
 *
 * @param _status The status value.
 * @return The kind.
 */
#define STATUS_KIND(_status) (((_status) >> 8) & 0xFF)

/**
 * @brief Extract the detail code from a status value.
 *
 * @param _status The status value.
 * @return The detail code.
 */
#define STATUS_DETAIL(_status) ((_status) & 0xFF)

/**
 * @brief Check if a status indicates success.
 *
 * @param _status The status value.
 * @return True if success, false otherwise.
 */
#define IS_INFO(_status) (ST_SEV(_status) == ST_SEV_INFO)

/**
 * @brief Check if a status indicates an error.
 *
 * @param _status The status value.
 * @return True if error, false otherwise.
 */
#define IS_ERR(_status) (ST_SEV(_status) == ST_SEV_ERR)

/**
 * @brief Check if a status matches a specific code.
 *
 * @param _status The status value.
 * @param _code The code to check against (without STATUS_CODE_ prefix).
 * @return True if match, false otherwise.
 */
#define IS_CODE(_status, _code) (STATUS_CODE(_status) == STATUS_CODE_##_code)

/**
 * @brief Check if a status matches a specific kind.
 *
 * @param _status The status value.
 * @param _kind The kind to check against (without STATUS_KIND_ prefix).
 * @return True if match, false otherwise.
 */
#define IS_KIND(_status, _kind) (STATUS_KIND(_status) == STATUS_KIND_##_kind)

/**
 * @brief Check if a status matches a specific severity.
 *
 * @param _status The status value.
 * @param _sev The severity to check against (without ST_SEV_ prefix).
 * @return True if match, false otherwise.
 */
#define IS_SEV(_status, _sev) (ST_SEV(_status) == ST_SEV_##_sev)

/**
 * @brief Check if a status matches a specific source.
 *
 * @param _status The status value.
 * @param _src The source to check against (without STATUS_SRC_ prefix).
 * @return True if match, false otherwise.
 */
#define IS_SRC(_status, _src) (STATUS_SRC(_status) == STATUS_SRC_##_src)

/**
 * @brief Retry an expression while it returns an error.
 *
 * @param _expr The expression to evaluate.
 * @return The final status.
 */
#define RETRY(_expr) \
    ({ \
        status_t _retry; \
        do \
        { \
            _retry = (_expr); \
        } while (IS_ERR(_retry)); \
        _retry; \
    })

/**
 * @brief Retry an expression a specific number of times while it returns an error.
 *
 * @param _expr The expression to evaluate.
 * @param _n The maximum number of retries.
 * @return The final status.
 */
#define RETRY_N(_expr, _n) \
    ({ \
        status_t _retry; \
        uint64_t _count = (_n); \
        do \
        { \
            _retry = (_expr); \
        } while (IS_ERR(_retry) && (_count > 1 ? (--_count, 1) : 0)); \
        _retry; \
    })

/**
 * @brief Retry an expression while it returns a specific error code.
 *
 * @param _expr The expression to evaluate.
 * @param _code The error code to retry on.
 * @return The final status.
 */
#define RETRY_ON_CODE(_expr, _code) \
    ({ \
        status_t _retry; \
        do \
        { \
            _retry = (_expr); \
        } while (IS_ERR(_retry) && IS_CODE(_retry, _code)); \
        _retry; \
    })

/**
 * @brief Retry an expression while it returns a specific severity.
 *
 * @param _expr The expression to evaluate.
 * @param _sev The severity to retry on.
 * @return The final status.
 */
#define RETRY_ON_SEV(_expr, _sev) \
    ({ \
        status_t _retry; \
        do \
        { \
            _retry = (_expr); \
        } while (ST_SEV(_retry) == (_sev)); \
        _retry; \
    })

/**
 * @brief Status OK constant.
 */
#define OK STATUS(ST_SEV_INFO, STATUS_SRC_NONE, STATUS_CODE_NONE)

/**
 * @brief Create an information status.
 *
 * @param _source The source of the status (without STATUS_SRC_ prefix).
 * @param _code The code of the status (without STATUS_CODE_ prefix).
 */
#define INFO(_source, _code) STATUS(ST_SEV_INFO, STATUS_SRC_##_source, STATUS_CODE_##_code)

/**
 * @brief Create an error status.
 *
 * @param _source The source of the status (without STATUS_SRC_ prefix).
 * @param _code The code of the status (without STATUS_CODE_ prefix).
 */
#define ERR(_source, _code) STATUS(ST_SEV_ERR, STATUS_SRC_##_source, STATUS_CODE_##_code)

/**
 * @brief Convert a status severity to a string.
 *
 * @param sev The severity
 * @return The severity string.
 */
const char* status_sev_str(status_sev_t sev);

/**
 * @brief Convert a status source to a string.
 *
 * @param src The source.
 * @return The source string.
 */
const char* status_src_str(status_src_t src);

/**
 * @brief Convert a status kind to a string.
 *
 * @param kind The kind.
 * @return The kind string.
 */
const char* status_kind_str(status_kind_t kind);

/**
 * @brief Convert a status code to a string.
 *
 * @param code The code.
 * @return The code string.
 */
const char* status_code_str(status_code_t code);

/**
 * @brief Convert a status value to a POSIX errno value.
 *
 * @param status The status value to convert.
 * @return The corresponding POSIX errno value.
 */
int status_to_errno(status_t status);

#if defined(__cplusplus)
}
#endif

#endif

/** @} */
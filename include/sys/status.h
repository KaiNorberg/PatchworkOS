#ifndef _SYS_STATUS_H
#define _SYS_STATUS_H 1

#include <stdint.h>

/**
 * @brief System status codes.
 * @defgroup libstd_sys_status Status
 * @ingroup libstd
 *
 * The status system is used to report errors, warnings, and informational messages from various functions and
 * subsystems.
 *
 * ## Format
 *
 * A status is made up of a severity, source and code values. Included is a table describing the bit
 * format of a status value:
 *
 * | Bit(s) | Description  |
 * | :----- | :----------- |
 * | 31     | Severity bit |
 * | 16-30  | Source       |
 * | 15-0   | Code         |
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
 * @enum st_sev_t
 *
 * Specifies the severity of a status.
 */
typedef enum
{
    ST_SEV_OK = 0,  ///< Success/Informational.
    ST_SEV_ERR = 1, ///< Error.
} st_sev_t;

/**
 * @brief Status source.
 * @enum st_src_t
 *
 * Specifies they layer of an operation or subsystem, that generated the status.
 *
 * @note This is not used to group status values, a `ST_CODE_NOMEM` does not necessarily have a `ST_CODE_MEM` source.
 */
typedef enum
{
    ST_SRC_NONE,    ///< No specific source.
    ST_SRC_IO,      ///< Input/Output.
    ST_SRC_MEM,     ///< Memory management, primarily the Physical Memory Manager.
    ST_SRC_MMU,     ///< Memory Management Unit, used by subsystems related to virtual memory.
    ST_SRC_SIMD,    ///< SIMD operations.
    ST_SRC_SCHED,   ///< Scheduler.
    ST_SRC_INT,     ///< Interrupts.
    ST_SRC_SYNC,    ///< Synchronization primitives.
    ST_SRC_DRIVER,  ///< Device drivers.
    ST_SRC_FS,      ///< Filesystem.
    ST_SRC_VFS,     ///< Virtual Filesystem.
    ST_SRC_IPC,     ///< Inter-Process Communication.
    ST_SRC_LIBSTD,  ///< Userspace Standard Library.
    ST_SRC_USER,    ///< Userspace Program.
    ST_SRC_PROC,    ///< Process Subsystem.
    ST_SRC_MODULE,  ///< Module Loader.
    ST_SRC_PORT,    ///< CPU Port I/O.
    ST_SRC_SYSCALL, ///< Syscall.
    ST_SRC_ACPI,    ///< ACPI.
    ST_SRC_TEST,   ///< Testing.
} st_src_t;

/**
 * @brief Status code.
 * @enum st_code_t
 *
 * Specifies the specific error or status condition.
 */
typedef enum
{
    ST_CODE_NONE,             ///< No specific code.
    ST_CODE_UNKNOWN,          ///< Unknown error.
    ST_CODE_INVAL,            ///< Invalid argument.
    ST_CODE_OVERFLOW,         ///< Buffer overflow.
    ST_CODE_TOOBIG,           ///< Value too big.
    ST_CODE_NOMEM,            ///< Out of memory.
    ST_CODE_TIMEOUT,          ///< Operation timed out.
    ST_CODE_NOSPACE,          ///< No space left.
    ST_CODE_MJ_OVERFLOW,      ///< Major number overflow.
    ST_CODE_MJ_NOSYS,         ///< Major number not found.
    ST_CODE_CANCELLED,        ///< Operation cancelled.
    ST_CODE_NOT_CANCELLABLE,  ///< Operation cannot be cancelled.
    ST_CODE_FAULT,            ///< Bad address.
    ST_CODE_DYING,            ///< Process is dying.
    ST_CODE_ACCESS,           ///< Permission denied.
    ST_CODE_ALIGN,            ///< Alignment error.
    ST_CODE_MAPPED,           ///< Already mapped.
    ST_CODE_UNMAPPED,         ///< Not mapped.
    ST_CODE_PINNED,           ///< Page pinned.
    ST_CODE_SHARED_LIMIT,     ///< Shared memory limit reached.
    ST_CODE_IN_STACK,         ///< Address in stack.
    ST_CODE_IMPL,             ///< Implementation error.
    ST_CODE_AGAIN,            ///< Resource temporarily unavailable.
    ST_CODE_INTR,             ///< Interrupted system call.
    ST_CODE_PATHTOOLONG,      ///< Path too long.
    ST_CODE_NAMETOOLONG,      ///< Name too long.
    ST_CODE_INVALCHAR,        ///< Invalid character.
    ST_CODE_INVALFLAG,        ///< Invalid flag.
    ST_CODE_CHANGED,          ///< State changed.
    ST_CODE_FULL,             ///< Buffer full.
    ST_CODE_MORE,             ///< More data is available then what was returned.
    ST_CODE_FD_OVERFLOW,      ///< File descriptor is over the maximum value.
    ST_CODE_MFILE,            ///< Too many file descriptors open.
    ST_CODE_BADFD,            ///< File descriptor is not open.
    ST_CODE_RAND,             ///< Hardware random number generator error.
    ST_CODE_NOENT,            ///< No such file or directory.
    ST_CODE_NOTDIR,           ///< Not a directory.
    ST_CODE_ISDIR,            ///< Is a directory.
    ST_CODE_BUSY,             ///< Device or resource busy.
    ST_CODE_EXIST,            ///< File exists.
    ST_CODE_XDEV,             ///< Cross-device link.
    ST_CODE_NOTEMPTY,         ///< Directory not empty.
    ST_CODE_NODEV,            ///< No such device.
    ST_CODE_IO,               ///< I/O error.
    ST_CODE_SHADOW_LIMIT,     ///< Maximum shadow mount depth reached.
    ST_CODE_LOOP,             ///< Too many levels of symbolic links.
    ST_CODE_NOFS,             ///< No filesystem found.
    ST_CODE_NEGATIVE,         ///< Path component does not exist.
    ST_CODE_ARGC,             ///< Invalid argument count.
    ST_CODE_INVALCTL,         ///< Invalid control command.
    ST_CODE_NOGROUP,          ///< Not within a group.
    ST_CODE_PERM,             ///< Operation not permitted.
    ST_CODE_NOTTY,            ///< Inappropriate ioctl for device.
    ST_CODE_SPIPE,            ///< Invalid seek.
    ST_CODE_MCLOCK,           ///< Too many clock sources.
    ST_CODE_TOCTOU,           ///< Time-of-check to time-of-use race condition.
    ST_CODE_INVALELF,         ///< Invalid ELF executable.
    ST_CODE_NOT_INIT,         ///< Resource is not initialized.
    ST_CODE_ALREADY_INIT,     ///< Resource is already initialized.
    ST_CODE_ACQUIRED,         ///< Resource is already acquired.
    ST_CODE_MTIMER,           ///< To many timer sources.
    ST_CODE_ILSEQ,            ///< Invalid byte sequence.
    ST_CODE_NO_ACPI_TABLE,    ///< Unable to locate ACPI table.
    ST_CODE_INVAL_ACPI_TABLE, ///< Invalid ACPI table.
    ST_CODE_DEADLOCK,             ///< Deadlock detected.
    ST_CODE_NO_BOOT_INFO,     ///< Bootloader did not provide needed info.
    ST_CODE_TEST_FAIL,        ///< Test failure.
} st_code_t;

/**
 * @brief Create a status value.
 *
 * @param _severity The severity of the status.
 * @param _source The source of the status.
 * @param _code The specific code of the status.
 * @return The constructed status value.
 */
#define STATUS(_severity, _source, _code) \
    ((status_t)(((uint32_t)(_severity) & 0x1) << 31) | (((uint32_t)(_source) & 0x7FFF) << 16) | \
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
#define ST_SRC(_status) (((_status) >> 16) & 0x7FFF)

/**
 * @brief Extract the code from a status value.
 *
 * @param _status The status value.
 * @return The code.
 */
#define ST_CODE(_status) ((_status) & 0xFFFF)

/**
 * @brief Check if a status indicates success.
 *
 * @param _status The status value.
 * @return True if success, false otherwise.
 */
#define IS_OK(_status) (ST_SEV(_status) == ST_SEV_OK)

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
 * @param _code The code to check against (without ST_CODE_ prefix).
 * @return True if match, false otherwise.
 */
#define IS_CODE(_status, _code) (ST_CODE(_status) == ST_CODE_##_code)

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
 * @param _src The source to check against (without ST_SRC_ prefix).
 * @return True if match, false otherwise.
 */
#define IS_SRC(_status, _src) (ST_SRC(_status) == ST_SRC_##_src)

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
        } while (IS_ERR(_retry) && ST_CODE(_retry) == (_code)); \
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
#define OK STATUS(ST_SEV_OK, ST_SRC_NONE, ST_CODE_NONE)

/**
 * @brief Create an information status.
 *
 * @param _source The source of the status (without ST_SRC_ prefix).
 * @param _code The code of the status (without ST_CODE_ prefix).
 */
#define INFO(_source, _code) STATUS(ST_SEV_OK, ST_SRC_##_source, ST_CODE_##_code)

/**
 * @brief Create an error status.
 *
 * @param _source The source of the status (without ST_SRC_ prefix).
 * @param _code The code of the status (without ST_CODE_ prefix).
 */
#define ERR(_source, _code) STATUS(ST_SEV_ERR, ST_SRC_##_source, ST_CODE_##_code)

/**
 * @brief Convert a status severity to a string.
 *
 * @param sev The severity
 * @return The severity string.
 */
const char* sevtostr(st_sev_t sev);

/**
 * @brief Convert a status source to a string.
 *
 * @param src The source.
 * @return The source string.
 */
const char* srctostr(st_src_t src);

/**
 * @brief Convert a status code to a string.
 *
 * @param code The code.
 * @return The code string.
 */
const char* codetostr(st_code_t code);

/** @} */

#endif
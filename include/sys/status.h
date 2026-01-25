#ifndef _SRC_STATUS_H
#define _SRC_STATUS_H 1

#include <stdint.h>

/**
 * @brief System status codes.
 * @defgroup libstd_sys_status Status
 * @ingroup libstd
 *
 * @{
 */

/**
 * @brief Status value.
 * @struct status_t
 */
typedef uint32_t status_t;

typedef enum
{
    ST_SEV_OK,
    ST_SEV_INFO,
    ST_SEV_WARN,
    ST_SEV_ERR,
} st_sev_t;

typedef enum
{
    ST_SRC_NONE,
    ST_SRC_IO,
    ST_SRC_MEM,
    ST_SRC_MMU,
    ST_SRC_SIMD,
    ST_SRC_SCHED,
    ST_SRC_INT,
    ST_SRC_SYNC,
    ST_SRC_DRIVER,
    ST_SRC_FS,
    ST_SRC_VFS,
    ST_SRC_IPC
} st_src_t;

typedef enum
{
    ST_CODE_NONE,
    ST_CODE_UNKNOWN,
    ST_CODE_INVAL,
    ST_CODE_OVERFLOW,
    ST_CODE_TOOBIG,
    ST_CODE_NOMEM,
    ST_CODE_TIMEOUT,
    ST_CODE_NOSPACE,
    ST_CODE_MJ_OVERFLOW,
    ST_CODE_MJ_NOSYS,
    ST_CODE_CANCELLED,
    ST_CODE_NOT_CANCELLABLE,
    ST_CODE_FAULT,
    ST_CODE_DYING,
    ST_CODE_ACCESS,
    ST_CODE_ALIGN,
    ST_CODE_MAPPED,
    ST_CODE_UNMAPPED,
    ST_CODE_PINNED,
    ST_CODE_SHARED_LIMIT,
    ST_CODE_IN_STACK,
    ST_CODE_IMPL,
    ST_CODE_AGAIN,
    ST_CODE_INTR,
    ST_CODE_PATHTOOLONG,
    ST_CODE_NAMETOOLONG,
    ST_CODE_INVALCHAR,
    ST_CODE_INVALFLAG,
    ST_CODE_CHANGED,
    ST_CODE_FULL,
} st_code_t;

#define STATUS(_severity, _source, _code) \
    ((status_t)(((uint32_t)(_severity) & 0x3) << 30) | (((uint32_t)(_source) & 0x1FFF) << 16) | ((uint32_t)(_code) & 0xFFFF))

#define ST_SEV(_status) (((_status) >> 30) & 0x3)
#define ST_CUST(_status) (((_status) >> 29) & 0x1)
#define ST_SRC(_status)   (((_status) >> 16) & 0x1FFF)
#define ST_CODE(_status)     ((_status) & 0xFFFF)

#define IS_OK(_status) (ST_SEV(_status) == ST_SEV_OK)
#define IS_FAIL(_status) (ST_SEV(_status) != ST_SEV_OK)
#define IS_INFO(_status)    (ST_SEV(_status) == ST_SEV_INFO)
#define IS_WARN(_status) (ST_SEV(_status) == ST_SEV_WARN)
#define IS_ERR(_status)   (ST_SEV(_status) == ST_SEV_ERR)

#define RETRY(_expr) \
    ({ \
        status_t _retry; \
        do \
        { \
            _retry = (_expr); \
        } while (IS_FAIL(_retry)); \
        _retry; \
    })

#define RETRY_N(_expr, _n) \
    ({ \
        status_t _retry; \
        uint64_t _count = (_n); \
        do \
        { \
            _retry = (_expr); \
        } while (IS_FAIL(_retry) && (_count > 1 ? (--_count, 1) : 0)); \
        _retry; \
    })

#define RETRY_ON_CODE(_expr, _code) \
    ({ \
        status_t _retry; \
        do \
        { \
            _retry = (_expr); \
        } while (IS_FAIL(_retry) && ST_CODE(_retry) == (_code)); \
        _retry; \
    })

#define RETRY_ON_SEV(_expr, _sev) \
    ({ \
        status_t _retry; \
        do \
        { \
            _retry = (_expr); \
        } while (ST_SEV(_retry) == (_sev)); \
        _retry; \
    })

#define OK              STATUS(ST_SEV_OK, ST_SRC_NONE, ST_CODE_NONE)
#define INFO(_source, _code) STATUS(ST_SEV_INFO, ST_SRC_##_source, ST_CODE_##_code)
#define WARN(_source, _code) STATUS(ST_SEV_WARN, ST_SRC_##_source, ST_CODE_##_code)
#define ERR(_source, _code)  STATUS(ST_SEV_ERR, ST_SRC_##_source, ST_CODE_##_code)

/** @} */

#endif
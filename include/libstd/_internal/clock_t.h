#ifndef _AUX_CLOCK_T_H
#define _AUX_CLOCK_T_H 1

/**
 * @brief A nanosecond time.
 * @ingroup libstd
 *
 * The `clock_t` type is extended in Patchwork to respresent any nanosecond time. The special value `CLOCKS_PER_SEC`
 * is inherited from the C standard library but Patchwork also defines the special value `CLOCKS_NEVER` that all
 * functions and system calls that take in a timeout are expected to handle.
 *
 */
typedef __UINT64_TYPE__ clock_t;

#define CLOCKS_PER_SEC ((clock_t)1000000000ULL)
#define CLOCKS_NEVER ((clock_t)__UINT64_MAX__)

/**
 * @brief Safely calculate remaining time until deadline.
 * @ingroup libstd
 *
 * Handles `CLOCKS_NEVER` and avoids unsigned integer underflow when deadline has passed.
 *
 * @param deadline The deadline timestamp.
 * @param uptime The current uptime.
 * @return The remaining time, `0` if deadline passed, or `CLOCKS_NEVER` if deadline is `CLOCKS_NEVER`.
 */
#define CLOCKS_REMAINING(deadline, uptime) \
    ({ \
        clock_t _deadline = (deadline); \
        clock_t _uptime = (uptime); \
        ((_deadline) == CLOCKS_NEVER ? CLOCKS_NEVER : ((_deadline) > (_uptime) ? (_deadline) - (_uptime) : 0)); \
    })

/**
 * @brief Safely calculate deadline from timeout.
 * @ingroup libstd
 *
 * Handles `CLOCKS_NEVER` and avoids unsigned integer overflow.
 *
 * @param timeout The timeout duration.
 * @param uptime The current uptime.
 * @return The deadline timestamp, or `CLOCKS_NEVER` if timeout is `CLOCKS_NEVER` or would overflow.
 */
#define CLOCKS_DEADLINE(timeout, uptime) \
    ({ \
        clock_t _timeout = (timeout); \
        clock_t _uptime = (uptime); \
        ((_timeout) == CLOCKS_NEVER \
                ? CLOCKS_NEVER \
                : ((_timeout) > CLOCKS_NEVER - (_uptime) ? CLOCKS_NEVER : (_uptime) + (_timeout))); \
    })

#endif

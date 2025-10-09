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

#endif

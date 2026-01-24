#ifndef _INTERNAL_SSIZE_T_H
#define _INTERNAL_SSIZE_T_H 1

/**
 * @brief Signed size type.
 * @ingroup libstd
 *
 * The `ssize_t` is used to represent offsets in memory.
 *
 */
typedef __INT64_TYPE__ ssize_t;

#define SSIZE_MAX __INT64_MAX__
#define SSIZE_MIN __INT64_MIN__

#endif

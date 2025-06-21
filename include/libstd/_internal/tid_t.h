#ifndef _AUX_TID_T_H
#define _AUX_TID_T_H 1

/**
 * @brief Thread Identifier.
 * @ingroup libstd
 *
 * The `tid_t` type is used to store thread identifiers, note that these id's are only used by the kernel and its system
 * calls, for libstd threading found in `thread.h` the `thrd_t` type should be used.
 *
 */
typedef __UINT64_TYPE__ tid_t;

#endif

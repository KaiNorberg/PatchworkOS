#ifndef _INTERNAL_FD_T_H
#define _INTERNAL_FD_T_H 1

/**
 * @addtogroup libstd
 *
 * @{
 */

typedef __UINT64_TYPE__ fd_t; ///< File descriptor type.

#define FD_NONE ((fd_t) - 1) ///< No file descriptor.

#define FD_CWD ((fd_t) - 2) ///< Use the current working directory.)

/** @} */

#endif

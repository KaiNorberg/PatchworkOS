#ifndef _AUX_FD_T_H
#define _AUX_FD_T_H 1

/**
 * @brief A file descriptor.
 * @ingroup libstd
 *
 * The `fd_t` type represents a file descriptor, which is a index into the processes files table. We also define the
 * special value `FD_NONE` which is equal to `UINT64_MAX` to represent no file descriptor.
 *
 */
typedef __UINT64_TYPE__ fd_t;

/**
 * @brief No file descriptor constant.
 * @ingroup libstd
 * @def FD_NONE
 *
 * The `FD_NONE` value represents no file descriptor.
 *
 */
#define FD_NONE __UINT64_MAX__

#endif

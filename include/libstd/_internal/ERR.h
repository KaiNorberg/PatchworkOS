#ifndef _AUX_ERR_H
#define _AUX_ERR_H 1

/**
 * @brief Integer error value.
 * @ingroup libstd
 *
 * The `ERR` value is returned from system calls when an error has occurred, when that happens the `SYS_ERRNO`
 * system call can be used to retrieve the errno code for the error that occurred. Some functions will also return
 * `ERR`, you should refer to the documentation of each function, however if a function returns a `uint64_t` and can
 * error then it should return `ERR` when an error occurs.
 *
 * The value is written using a binary representation to allow both unsigned and signed values to be compared without
 * weird behaviour but is equal to `UINT64_MAX`.
 *
 */
#define ERR (0b1111111111111111111111111111111111111111111111111111111111111111)

#endif

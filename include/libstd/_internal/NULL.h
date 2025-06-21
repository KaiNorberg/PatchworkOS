#ifndef _AUX_NULL_H
#define _AUX_NULL_H 1

#ifdef __cplusplus
#if __cplusplus >= 201103L
#define _NULL nullptr
#else
#define _NULL 0
#endif
#else
#define _NULL ((void*)0)
#endif

/**
 * @brief Pointer error value.
 * @ingroup libstd
 *
 * The `NULL` value respresents a invalid pointer, just as expected in the C standard library, but system calls that
 * return pointers will also return `NULL` when an error has occurred, when that happens the `SYS_LAST_ERROR` system
 * call can be used to retrieve the errno code for the error that occurred.
 *
 */
#define NULL _NULL

#endif
#ifndef _INTERNAL_H
#define _INTERNAL_H 1

#if defined(__cplusplus)
extern "C" {
#endif
 
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus) || !defined(__STDC_VERSION) || __STDC_VERSION__ < 199901L
#define LIBC_RESTRICT
#define LIBC_INLINE
#else
#define LIBC_RESTRICT restrict
#define LIBC_INLINE inline
#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif
 
#endif
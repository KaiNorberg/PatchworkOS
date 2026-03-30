#ifndef _SETJMP_H
#define _SETJMP_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libc/config.h"

/**
 * @brief Setjmp/Longjmp functions.
 * @defgroup libc_setjmp Setjmp/Longjmp
 * @ingroup libc
 *
 * @todo Add signal handling and 100 other things later.
 *
 * @{
 */

typedef long jmp_buf[10];

int setjmp(jmp_buf env);
_NORETURN void longjmp(jmp_buf env, int value);

#define _setjmp(env) setjmp(env)
#define _longjmp(env, value) longjmp(env, value)

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

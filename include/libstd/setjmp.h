#ifndef _SETJMP_H
#define _SETJMP_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/config.h"

// TODO: Add signal handling and 100 other things

typedef long jmp_buf[10];

_PUBLIC int setjmp(jmp_buf env);
_PUBLIC void longjmp(jmp_buf env, int value);

#define _setjmp(env) setjmp(env)
#define _longjmp(env, value) longjmp(env, value)

#if defined(__cplusplus)
}
#endif

#endif

#ifndef _SIGNAL_H
#define _SIGNAL_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdatomic.h>

#include "_internal/config.h"

/**
 * @brief Wrappers around "notes" for ANSI C signal handling.
 * @defgroup libstd_signal Signal Handling
 * @ingroup libstd
 * 
 * For the same of compatibility with ANSI C, we provide these wrappers around "notes" for signal handling. However, it is preferred to use the native "notes" for IPC instead if possible.
 * 
 * @{
 */

#define SIGABRT 1
#define SIGFPE 2
#define SIGILL 3
#define SIGINT 4
#define SIGSEGV 5
#define SIGTERM 6
#define SIGMAX 32

#define SIG_DFL ((void (*)(int))0)
#define SIG_ERR ((void (*)(int))-1)
#define SIG_IGN ((void (*)(int))1)

typedef _Atomic(int) sig_atomic_t;

typedef void (*sighandler_t)(int);

_PUBLIC sighandler_t signal(int sig, sighandler_t func);

_PUBLIC int raise(int sig);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif

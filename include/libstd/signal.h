#ifndef _SIGNAL_H
#define _SIGNAL_H 1

#include "_internal/config.h"

/* Abnormal termination / abort() */
#define SIGABRT 6
/* Arithmetic exception / division by zero / overflow */
#define SIGFPE 8
/* Illegal instruction */
#define SIGILL 4
/* Interactive attention signal */
#define SIGINT 2
/* Invalid memory access */
#define SIGSEGV 11
/* Termination request */
#define SIGTERM 15

#define SIG_DFL (void (*)(int))0
#define SIG_ERR (void (*)(int)) - 1
#define SIG_IGN (void (*)(int))1

typedef __SIG_ATOMIC_TYPE__ sig_atomic_t;

_PUBLIC void (*signal(int sig, void (*func)(int)))(int);

_PUBLIC int raise(int sig);

#endif

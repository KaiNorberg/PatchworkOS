#ifndef _STDLIB_H
#define _STDLIB_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

__attribute__((__noreturn__)) void exit(int status);

#if defined(__cplusplus)
}
#endif

#endif
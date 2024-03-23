#ifndef _INTERNAL_SYSCALLS_H
#define _INTERNAL_SYSCALLS_H 1

#include <stdint.h>
#include <errno.h>

__attribute__((__noreturn__)) void _ProcessExit(uint64_t status);

uint64_t _Sleep(uint64_t nanoseconds);

errno_t _KernelErrno(void);

uint64_t _Test(const char* string);

#endif
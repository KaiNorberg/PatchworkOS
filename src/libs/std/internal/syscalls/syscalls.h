#ifndef _INTERNAL_SYSCALLS_H
#define _INTERNAL_SYSCALLS_H 1

#include <stdint.h>
#include <errno.h>

__attribute__((__noreturn__)) void sys_exit_process(uint64_t status);

errno_t sys_error();

uint64_t sys_test(const char* string);

#endif
#ifndef _INTERNAL_SYSCALLS_H 
#define _INTERNAL_SYSCALLS_H 1

#include <stdint.h>

__attribute__((__noreturn__)) void sys_exit_process(uint64_t status);

uint64_t sys_test(const char* string);

#endif
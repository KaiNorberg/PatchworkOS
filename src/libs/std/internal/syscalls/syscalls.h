#ifndef _INTERNAL_SYSCALLS_H
#define _INTERNAL_SYSCALLS_H 1

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sys/io.h>

#define SYS_PROCESS_EXIT 0
#define SYS_SPAWN 1
#define SYS_SLEEP 2
#define SYS_ALLOCATE 3
#define SYS_KERNEL_ERRNO 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_READ 7
#define SYS_WRITE 8
#define SYS_SEEK 9
#define SYS_TEST 10

uint64_t _Syscall0();
uint64_t _Syscall1();
uint64_t _Syscall2();
uint64_t _Syscall3();
uint64_t _Syscall4();
uint64_t _Syscall5();

#define SYSCALL(selector, n, ...) _Syscall##n(__VA_ARGS__ __VA_OPT__(,) selector)

#endif
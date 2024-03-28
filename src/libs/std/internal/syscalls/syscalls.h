#ifndef _INTERNAL_SYSCALLS_H
#define _INTERNAL_SYSCALLS_H 1

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sys/io.h>

#define SYS_PROCESS_EXIT 0
#define SYS_THREAD_EXIT 1
#define SYS_SPAWN 2
#define SYS_SLEEP 3
#define SYS_ALLOCATE 4
#define SYS_ERROR 5
#define SYS_OPEN 6
#define SYS_CLOSE 7
#define SYS_READ 8
#define SYS_WRITE 9
#define SYS_SEEK 10
#define SYS_TEST 11

extern uint64_t _Syscall0();
extern uint64_t _Syscall1();
extern uint64_t _Syscall2();
extern uint64_t _Syscall3();
extern uint64_t _Syscall4();
extern uint64_t _Syscall5();

#define SYSCALL(selector, n, ...) _Syscall##n(__VA_ARGS__ __VA_OPT__(,) selector)

#endif
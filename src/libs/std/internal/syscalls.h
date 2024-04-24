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
#define SYS_ERROR 4
#define SYS_PID 5
#define SYS_TID 6
#define SYS_OPEN 7
#define SYS_CLOSE 8
#define SYS_READ 9
#define SYS_WRITE 10
#define SYS_SEEK 11
#define SYS_TEST 12

extern uint64_t _Syscall0();
extern uint64_t _Syscall1();
extern uint64_t _Syscall2();
extern uint64_t _Syscall3();
extern uint64_t _Syscall4();
extern uint64_t _Syscall5();

#define SYSCALL(selector, n, ...) _Syscall##n(__VA_ARGS__ __VA_OPT__(,) selector)

#endif
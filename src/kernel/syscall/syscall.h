#pragma once

#define SYSCALL_VECTOR 0x80

typedef void(*Syscall)();

extern void syscall_handler();
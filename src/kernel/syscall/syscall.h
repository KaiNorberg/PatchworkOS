#pragma once

#include "page_directory/page_directory.h"
#include "interrupt_frame/interrupt_frame.h"

#define SYSCALL_GET_ARG1(self) (self->interruptFrame->rdi)
#define SYSCALL_GET_ARG2(self) (self->interruptFrame->rsi)
#define SYSCALL_GET_ARG3(self) (self->interruptFrame->rdx)
#define SYSCALL_GET_ARG4(self) (self->interruptFrame->rcx)
#define SYSCALL_GET_ARG5(self) (self->interruptFrame->r8)
#define SYSCALL_GET_ARG6(self) (self->interruptFrame->r9)

#define SYSCALL_VECTOR 0x80

typedef void(*Syscall)();

extern void syscall_handler();
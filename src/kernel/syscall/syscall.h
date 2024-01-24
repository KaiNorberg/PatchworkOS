#pragma once

#include <stdint.h>

#include "page_directory/page_directory.h"
#include "interrupts/interrupts.h"
#include "scheduler/scheduler.h"

#define SYSCALL_GET_ARG1(interruptFrame) (interruptFrame->rdi)
#define SYSCALL_GET_ARG2(interruptFrame) (interruptFrame->rsi)
#define SYSCALL_GET_ARG3(interruptFrame) (interruptFrame->rdx)
#define SYSCALL_GET_ARG4(interruptFrame) (interruptFrame->rcx)
#define SYSCALL_GET_ARG5(interruptFrame) (interruptFrame->r8)
#define SYSCALL_GET_ARG6(interruptFrame) (interruptFrame->r9)
#define SYSCALL_SET_RESULT(InterruptFrame, value) InterruptFrame->rax = value

#define SYSCALL_GET_PAGE_DIRECTORY(interruptFrame) ((PageDirectory*)interruptFrame->cr3)

#define SYSCALL_VECTOR 0x80

typedef void(*Syscall)(InterruptFrame* interruptFrame);

void syscall_exit(InterruptFrame* interruptFrame);

void syscall_fork(InterruptFrame* interruptFrame);

void syscall_sleep(InterruptFrame* interruptFrame);

void syscall_handler(InterruptFrame* interruptFrame);
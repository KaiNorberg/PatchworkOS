#pragma once

#include <stdint.h>

#include "page_directory/page_directory.h"
#include "interrupts/interrupts.h"
#include "multitasking/multitasking.h"

#define SYSCALL_GET_ARG1(frame) (frame->rdi)
#define SYSCALL_GET_ARG2(frame) (frame->rsi)
#define SYSCALL_GET_ARG3(frame) (frame->rdx)
#define SYSCALL_GET_ARG4(frame) (frame->rcx)
#define SYSCALL_GET_ARG5(frame) (frame->r8)
#define SYSCALL_GET_ARG6(frame) (frame->r9)

#define SYSCALL_GET_PAGE_DIRECTORY(frame) ((PageDirectory*)frame->cr3)

void syscall_init();

void syscall_handler(InterruptStackFrame* frame);
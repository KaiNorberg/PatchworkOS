.code64

#include "kernel/cpu/interrupt.h"
#include "kernel/cpu/gdt.h"
#include "kernel/cpu/cpu.h"

.extern syscall_handler
.extern note_handle_pending
.extern sched_do
.extern cpu_get

.global syscall_entry
.type syscall_entry, @function

.text

// rdi = first argument
// rsi = second argument
// rdx = third argument
// r10 = fourth argument
// r8  = fifth argument
// r9  = sixth argument
// rax = syscall number
// rcx = user rip
// r11 = user rflags
// Status in rax
// Result in rdx
syscall_entry:
    swapgs

    movq %rsp, %gs:CPU_OFFSET_USER_RSP
    movq %gs:CPU_OFFSET_SYSCALL_RSP, %rsp

    // Construct interrupt_frame_t on stack representing 
    // the state before the syscall
    pushq $GDT_SS_RING3
    pushq %gs:CPU_OFFSET_USER_RSP
    pushq %r11 // rflags
    pushq $GDT_CS_RING3
    pushq %rcx // rip

    // Dummy error and vector
    pushq $0
    pushq $0

    INTERRUPT_FRAME_REGS_PUSH

    sti

    // syscall_handler(rsp)
    movq %rsp, %rdi
    call syscall_handler

    INTERRUPT_FRAME_REGS_POP
    movq 8 * 5(%rsp), %rsp // Load user rsp from interrupt frame
    swapgs
    sysretq
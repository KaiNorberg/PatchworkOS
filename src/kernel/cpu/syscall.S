.code64
.intel_syntax noprefix

#include "kernel/cpu/interrupt.h"
#include "kernel/cpu/gdt.h"

#define GS_SYSCALL_RSP    0x0
#define GS_USER_RSP     0x8

.extern syscall_handler
.extern note_handle_pending
.extern sched_do
.extern cpu_get_unsafe

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
// Return value in rax
syscall_entry:
    swapgs
    mov qword ptr gs:[GS_USER_RSP], rsp
    mov rsp, qword ptr gs:[GS_SYSCALL_RSP]

    // Construct interrupt_frame_t on stack representing 
    // the state before the syscall
    push GDT_SS_RING3
    push qword ptr gs:[GS_USER_RSP]
    push r11 // rflags
    push GDT_CS_RING3
    push rcx // rip
    swapgs

    // Dummy error and vector
    push 0
    push 0

    INTERRUPT_FRAME_REGS_PUSH

    sti

    // syscall_handler(rsp)
    mov rdi, rsp
    call syscall_handler

    INTERRUPT_FRAME_REGS_POP
    mov rsp, [rsp + 8 * 5] // Load user rsp from interrupt frame
    sysretq
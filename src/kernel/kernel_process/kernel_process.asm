[bits 64]

global kernel_task_entry
global kernel_task_block

extern tss_kernel_stack

extern interrupt_common_return
extern local_scheduler_acquire
extern local_scheduler_schedule
extern local_scheduler_release

section .text

;rdi = entry
kernel_task_entry:
    cli

    call rdi
    ud2

;rdi = timeout
kernel_task_block:
    push rdi
    call tss_kernel_stack
    pop rdi
    mov rbp, rsp
    mov rsp, rax
    sti
    int 0x70
    cli
    mov rsp, rbp
    ret
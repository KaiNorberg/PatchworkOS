[bits 64]

KERNEL_TASK_BLOCK_VECTOR equ 0x70

global kernel_task_entry
global kernel_task_block

extern tss_kernel_stack

section .text

;rdi = entry
kernel_task_entry:
    call rdi
    ud2

;rdi = timeout
kernel_task_block:
    push rdi
    call tss_kernel_stack
    pop rdi
    pop r11
    mov rbp, rsp
    mov rsp, rax
    int KERNEL_TASK_BLOCK_VECTOR
    mov rsp, rbp
    jmp r11
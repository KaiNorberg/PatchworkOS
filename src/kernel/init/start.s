[bits 64]

extern kmain
extern kernel_early_init
extern kernel_init
extern thread_get_boot
extern thread_get_kernel_stack_top

section .text

; Entry point for the kernel. We end up here after the bootloader.
; rdi = bootloader information `boot_info_t*`
global _start
_start:
    cli
    cld

    mov rsp, kernel_stack_top
    and rsp, ~0xF
    xor rbp, rbp

    mov r15, rdi
    call kernel_early_init

    call thread_get_boot
    mov rdi, rax
    call thread_get_kernel_stack_top
    mov rsp, rax
    and rsp, ~0xF
    xor rbp, rbp

    mov rdi, r15
    call kernel_init

    call kmain

    ud2 ; Should never reach here

section .bss

global kernel_stack_bottom
kernel_stack_bottom:
resb 0x1000
global kernel_stack_top
kernel_stack_top:

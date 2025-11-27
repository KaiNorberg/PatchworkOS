[bits 64]

%include "kernel/cpu/interrupt.inc"

extern kmain
extern init_early
extern init
extern thread_get_boot
extern thread_load

extern bootInfo

global _start:function

section .text

; Entry point for the kernel. We end up here after the bootloader.
; rdi = bootloader information `boot_info_t*`
_start:
    cli
    cld

    ; rsp = early_init_stack_top
    mov rsp, early_init_stack_top

    ; bootInfo = rdi
    mov [bootInfo], rdi

    ; init_early()
    call init_early
    ud2 ; Should never be reached

section .bss align=0x1000
early_init_stack_bottom:
resb 0x1000
early_init_stack_top:

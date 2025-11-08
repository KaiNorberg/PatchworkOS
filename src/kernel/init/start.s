[bits 64]

%include "kernel/cpu/interrupt.inc"

extern kmain
extern init_early
extern init
extern thread_get_boot
extern thread_load

global _start:function

section .text

; Entry point for the kernel. We end up here after the bootloader.
; rdi = bootloader information `boot_info_t*`
_start:
    cli
    cld

    ; rsp = early_init_stack_top
    mov rsp, early_init_stack_top

    ; r15 = bootInfo
    mov r15, rdi
    call init_early

    ; r12 = thread_get_boot()
    call thread_get_boot
    mov r12, rax

    ; Allocate buffer for interrupt frame
    sub rsp, INTERRUPT_FRAME_SIZE

    ; thread_load(r12, rsp)
    mov rdi, r12
    mov rsi, rsp
    call thread_load

    ; The boot threads interrupt frame is now loaded on the stack, so we can now load all the registers of the boot thread
    ; and then jump to its entry point which is `kmain()`.
    ; Note that this will also trigger a page fault. But thats intended as we use page faults to dynamically grow the
    ; threads kernel stack and the stack starts out unmapped.
    INTERRUPT_FRAME_POP_AND_JUMP
    ud2 ; Should never be reached

section .bss align=0x1000
early_init_stack_bottom:
resb 0x1000
early_init_stack_top:

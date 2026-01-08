.code64
.intel_syntax noprefix

.extern init_early
.extern bootInfo

.global _start
.type _start, @function

.text

// Entry point for the kernel. We end up here after the bootloader.
// rdi = bootloader information `boot_info_t*`
_start:
    cli
    cld

    // rsp = early_init_stack_top
    lea rsp, [early_init_stack_top]

    // bootInfo = rdi
    mov [bootInfo], rdi

    // init_early()
    call init_early
    ud2 // Should never be reached

.section .bss
.align 0x1000
early_init_stack_bottom:
.skip 0x1000
early_init_stack_top:

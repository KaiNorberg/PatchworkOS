.code64

.extern init_early
.extern bootInfo
.extern _early_init_stack_end

.global _start
.type _start, @function

.text

// Entry point for the kernel. We end up here after the bootloader.
// rdi = bootloader information `boot_info_t*`
_start:
    cli
    cld

    // rsp = _early_init_stack_end
    movq $_early_init_stack_end, %rsp

    // bootInfo = rdi
    movq %rdi, bootInfo(%rip)

    // init_early()
    call init_early
    ud2 // Should never be reached

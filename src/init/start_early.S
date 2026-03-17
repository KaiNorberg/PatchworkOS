.code64

.extern _start

.global _start_early
.type _start_early, @function

.text

_start_early:
    mov $_stack_top, %rsp
    mov $_stack_top, %rbp

    // Fake stack frame for arguments
    pushq $0 // *argv
    pushq $0 // argc

    jmp _start

.end:

.bss
.align 64
_stack_bottom:
    .skip 4096 * 4
_stack_top:
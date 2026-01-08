.code64

.extern main
.extern exit
.extern _std_init

.global _start
.type _start, @function

.text

_start:
    movq %rsp, %rbp

    // Check kernel_sched_loader for the stack layout and register setup

    pushq %rsi
    pushq %rdi

    call _std_init

    popq %rdi
    popq %rsi

    call main
    pushq %rax

    popq %rdi
    call exit
    ud2
.end:
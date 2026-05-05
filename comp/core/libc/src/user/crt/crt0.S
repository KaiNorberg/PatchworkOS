.code64

.extern main
.extern exit
.extern _init
.extern _std_init
.extern _environ_set

.global _start
.type _start, @function

.text

_start:
    // _environ_set(envp)
    mov (%rsp), %rax
    lea 16(%rsp, %rax, 8), %rdi
    call _environ_set

    call _std_init

    call _init

    // main(argc, argv, envp)
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    mov %rdi, %rdx
    lea 16(%rsp, %rdx, 8), %rdx
    call main

    mov %rax, %rdi
    call exit
    ud2
.end:
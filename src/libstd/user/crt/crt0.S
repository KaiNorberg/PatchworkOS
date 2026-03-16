.code64

.extern main
.extern exit
.extern _init
.extern _std_init

.global _start
.type _start, @function

.text

_start:
    call _std_init

    call _init

    call main

    mov %rax, %rdi
    call exit
    ud2
.end:
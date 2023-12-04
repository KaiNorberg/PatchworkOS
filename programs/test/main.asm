global _start
_start:
    mov rax, 0 ;Syscall test!
    int 0x80

    mov rax, 0 ;Syscall test!
    int 0x80

    mov rax, 1 ;Yield
    int 0x80
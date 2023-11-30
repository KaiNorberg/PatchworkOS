section .text

extern currentTask

global switch_registers
global load_registers

switch_registers:
    cli ;Disable interupts

    ;Save registers to "from" in rdi
    mov [rdi + 0], rax
    mov [rdi + 8], rbx
    mov [rdi + 16], rcx
    mov [rdi + 24], rdx
    mov [rdi + 32], rsp
    mov [rdi + 40], rbp

    pop qword [rdi + 48] ;rip

    pushfq
    pop qword [rdi + 56] ;rflags

    mov rax, cr3
    mov [rdi + 64], rax ;cr3

    ;Save registers to "to" in rsi
    mov [rsi + 8], rbx
    mov [rsi + 16], rcx
    mov [rsi + 24], rdx

    push qword [rsi + 56]
    popfq ;flags

    mov [rsi + 32], rsp ;Stack pointer
    mov [rsi + 40], rbp ;Stack base pointer

    mov [rsi + 64], rax ;address space
    mov rax, cr3

    mov [rsi + 0], rax

    sti ;Enable interrupts

    jmp [rsi + 48] ;Instruction pointer

    ret
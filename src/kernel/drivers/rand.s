[bits 64]

section .text

; rdi: uint32_t* value
; rsi: uint8_t* retries
; return: On success, 0. On failure, `ERR`.
global rdrand_do
rdrand_do:
.retry:
    rdrand eax
    jc .success
    inc byte [rsi]
    cmp byte [rsi], 10
    jl .retry
    xor rax, rax
    not rax
    ret
.success:
    mov [rdi], eax
    mov rax, 0
    ret

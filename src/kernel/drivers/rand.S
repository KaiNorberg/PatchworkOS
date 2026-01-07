.code64
.intel_syntax noprefix

.global rdrand_do
.type rdrand_do, @function

.text

// rdi: uint32_t* value
// rsi: uint8_t* retries
// return: On success, 0. On failure, `ERR`.
rdrand_do:
.retry:
    rdrand eax
    jc .success
    inc byte ptr [rsi]
    cmp byte ptr [rsi], 10
    jl .retry
    xor rax, rax
    not rax
    ret
.success:
    mov [rdi], eax
    mov rax, 0
    ret

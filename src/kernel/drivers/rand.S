.code64

.global rdrand_do
.type rdrand_do, @function

.text

// rdi: uint32_t* value
// rsi: uint8_t* retries
// return: On success, 0. On failure, `_FAIL`.
rdrand_do:
.retry:
    rdrand %eax
    jc .success
    incb (%rsi)
    cmpb $10, (%rsi)
    jl .retry
    xorq %rax, %rax
    notq %rax
    ret
.success:
    movl %eax, (%rdi)
    movq $0, %rax
    ret

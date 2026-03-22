.code64

.global rdrand_do
.type rdrand_do, @function

.text

// rdi: uint32_t* value
// rsi: uint8_t* retries
// return: `true` on success, `false` otherwise.
rdrand_do:
.retry:
    rdrand %eax
    jc .success
    incb (%rsi)
    cmpb $10, (%rsi)
    jl .retry
    movq $0, %rax
    ret
.success:
    movl %eax, (%rdi)
    movq $1, %rax
    ret

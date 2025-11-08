[bits 64]

section .text

%ifndef _KERNEL_

global memcpy_sse2:function

; rdi = dest
; rsi = src
; rdx = n
; return rdi
memcpy_sse2:
    mov rax, rdi
    test rdx, rdx
    jz .done

.align_loop:
    test rdx, rdx
    jz .done
    test rdi, 15
    jz .aligned
    mov cl, byte [rsi]
    mov byte [rdi], cl
    inc rdi
    inc rsi
    dec rdx
    jnz .align_loop
.aligned:

    cmp rdx, 262144 ; 256KB
    jb .small_copy

.large_copy: ; Non temporal stores
    cmp rdx, 256
    jb .large_copy_done
    prefetchnta [rsi + 512]
    movdqu xmm0, [rsi]
    movdqu xmm1, [rsi + 16]
    movdqu xmm2, [rsi + 32]
    movdqu xmm3, [rsi + 48]
    movdqu xmm4, [rsi + 64]
    movdqu xmm5, [rsi + 80]
    movdqu xmm6, [rsi + 96]
    movdqu xmm7, [rsi + 112]
    movdqu xmm8, [rsi + 128]
    movdqu xmm9, [rsi + 144]
    movdqu xmm10, [rsi + 160]
    movdqu xmm11, [rsi + 176]
    movdqu xmm12, [rsi + 192]
    movdqu xmm13, [rsi + 208]
    movdqu xmm14, [rsi + 224]
    movdqu xmm15, [rsi + 240]
    movntdq [rdi], xmm0
    movntdq [rdi + 16], xmm1
    movntdq [rdi + 32], xmm2
    movntdq [rdi + 48], xmm3
    movntdq [rdi + 64], xmm4
    movntdq [rdi + 80], xmm5
    movntdq [rdi + 96], xmm6
    movntdq [rdi + 112], xmm7
    movntdq [rdi + 128], xmm8
    movntdq [rdi + 144], xmm9
    movntdq [rdi + 160], xmm10
    movntdq [rdi + 176], xmm11
    movntdq [rdi + 192], xmm12
    movntdq [rdi + 208], xmm13
    movntdq [rdi + 224], xmm14
    movntdq [rdi + 240], xmm15
    add rdi, 256
    add rsi, 256
    sub rdx, 256
    jmp .large_copy

.large_copy_done:
    sfence

.small_copy:
    cmp rdx, 64
    jb .copy_tail
    prefetchnta [rsi + 256]
    movdqu xmm0, [rsi]
    movdqu xmm1, [rsi + 16]
    movdqu xmm2, [rsi + 32]
    movdqu xmm3, [rsi + 48]
    movdqa [rdi], xmm0
    movdqa [rdi + 16], xmm1
    movdqa [rdi + 32], xmm2
    movdqa [rdi + 48], xmm3
    add rdi, 64
    add rsi, 64
    sub rdx, 64
    jmp .small_copy

.copy_tail:
    test rdx, rdx
    jz .done
    mov cl, byte [rsi]
    mov byte [rdi], cl
    inc rdi
    inc rsi
    dec rdx
    jmp .copy_tail

.done:

    ret

%endif ; _KERNEL_
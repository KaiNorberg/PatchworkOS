.code64

.global draw_blit_sse2
.type draw_blit_sse2, @function

.text

// Parameters (System V AMD64 ABI):
// rdi = (pixel_t*)dst
// rsi = (pixel_t*)src
// edx = (int32_t)width
// ecx = (int32_t)height
// r8d = (int32_t)dstPitch
// r9d = (int32_t)srcPitch

draw_blit_sse2:
    // Sign-extend 32-bit parameters to 64-bit to safely use them as array indices
    movsxd %edx, %rdx
    test %rdx, %rdx
    jle .Lend

    movsxd %ecx, %rcx
    test %rcx, %rcx
    jle .Lend
    
    movsxd %r8d, %r8
    movsxd %r9d, %r9

    // Generate constants
    pxor %xmm0, %xmm0           // xmm0 = 0 (used for unpacking bytes to words)

    pcmpeqw %xmm7, %xmm7
    psrlw $15, %xmm7            // xmm7 = 1 per word (used in DIV_255: x + 1)

    pcmpeqw %xmm6, %xmm6
    psrlw $8, %xmm6             // xmm6 = 255 per word (used for 255 - alpha)

    pcmpeqd %xmm5, %xmm5
    pslld $24, %xmm5            // xmm5 = 0xFF000000 per dword (sets destination alpha to 255)

.Lrow_loop:
    mov %rdx, %rax              // rax = remaining width in current row
    xor %r10, %r10              // r10 = pixel index offset (x)

.Lcol_loop:
    cmp $4, %rax
    jl .Ltail_loop              // If fewer than 4 pixels left, go to tail

    // 1. Load 4 pixels (16 bytes)
    movdqu (%rsi, %r10, 4), %xmm1 // src
    movdqu (%rdi, %r10, 4), %xmm2 // dst

    // 2. Unpack bytes to words (so 8-bit max 255 multiplication won't overflow)
    movdqa %xmm1, %xmm3
    movdqa %xmm2, %xmm4

    punpcklbw %xmm0, %xmm1      // src_lo (pixels 0, 1)
    punpckhbw %xmm0, %xmm3      // src_hi (pixels 2, 3)
    punpcklbw %xmm0, %xmm2      // dst_lo (pixels 0, 1)
    punpckhbw %xmm0, %xmm4      // dst_hi (pixels 2, 3)

    // 3. Extract and broadcast alpha channels for src_lo
    movdqa %xmm1, %xmm8
    pshuflw $0xFF, %xmm8, %xmm8 // Broadcast alpha of pixel 0 to all words in low 64-bits
    pshufhw $0xFF, %xmm8, %xmm8 // Broadcast alpha of pixel 1 to all words in high 64-bits

    // Extract and broadcast alpha channels for src_hi
    movdqa %xmm3, %xmm9
    pshuflw $0xFF, %xmm9, %xmm9 // Broadcast alpha of pixel 2
    pshufhw $0xFF, %xmm9, %xmm9 // Broadcast alpha of pixel 3

    // 4. Calculate invAlpha (255 - alpha)
    movdqa %xmm6, %xmm10
    psubw %xmm8, %xmm10         // invAlpha_lo
    movdqa %xmm6, %xmm11
    psubw %xmm9, %xmm11         // invAlpha_hi

    // 5. Multiply src by alpha and dst by invAlpha
    pmullw %xmm8, %xmm1         // src_lo * alpha_lo
    pmullw %xmm9, %xmm3         // src_hi * alpha_hi
    pmullw %xmm10, %xmm2        // dst_lo * invAlpha_lo
    pmullw %xmm11, %xmm4        // dst_hi * invAlpha_hi

    // 6. Add them together
    paddw %xmm2, %xmm1          // sum_lo
    paddw %xmm4, %xmm3          // sum_hi

    // 7. DIV_255 approximation: (((x) + 1 + ((x) >> 8)) >> 8)
    // DIV_255 for low 2 pixels
    movdqa %xmm1, %xmm8
    psrlw $8, %xmm8             // (x) >> 8
    paddw %xmm7, %xmm1          // (x) + 1
    paddw %xmm8, %xmm1          // (x) + 1 + ((x) >> 8)
    psrlw $8, %xmm1             // >> 8

    // DIV_255 for high 2 pixels
    movdqa %xmm3, %xmm9
    psrlw $8, %xmm9
    paddw %xmm7, %xmm3
    paddw %xmm9, %xmm3
    psrlw $8, %xmm3

    // 8. Pack back to 8-bit bytes
    packuswb %xmm3, %xmm1       // Pack words from xmm1 and xmm3 into bytes in xmm1

    // 9. Force alpha channel to 255 (as requested by C code: dstRow[x].a = 255)
    por %xmm5, %xmm1

    // 10. Store back 4 pixels
    movdqu %xmm1, (%rdi, %r10, 4)

    add $4, %r10                // Advance by 4 pixels
    sub $4, %rax                // Decrease width counter by 4
    jmp .Lcol_loop

.Ltail_loop:
    test %rax, %rax
    je .Lrow_end                // If no remaining pixels, end of row

    // Process 1 leftover pixel using the identical vector logic applied to 32 bits
    movd (%rsi, %r10, 4), %xmm1
    movd (%rdi, %r10, 4), %xmm2

    punpcklbw %xmm0, %xmm1
    punpcklbw %xmm0, %xmm2

    movdqa %xmm1, %xmm8
    pshuflw $0xFF, %xmm8, %xmm8

    movdqa %xmm6, %xmm10
    psubw %xmm8, %xmm10

    pmullw %xmm8, %xmm1
    pmullw %xmm10, %xmm2

    paddw %xmm2, %xmm1

    movdqa %xmm1, %xmm8
    psrlw $8, %xmm8
    paddw %xmm7, %xmm1
    paddw %xmm8, %xmm1
    psrlw $8, %xmm1

    packuswb %xmm1, %xmm1
    por %xmm5, %xmm1

    movd %xmm1, (%rdi, %r10, 4) // Store exactly 1 pixel (32 bits)

    inc %r10                    // Advance by 1 pixel
    dec %rax                    // Decrease width counter by 1
    jmp .Ltail_loop

.Lrow_end:
    add %r8, %rdi               // dst = (uint8_t*)dst + dstPitch
    add %r9, %rsi               // src = (uint8_t*)src + srcPitch
    dec %rcx                    // height--
    jnz .Lrow_loop              // Continue if height > 0

.Lend:
    ret
[bits 64]

section .text

global rflags_read
rflags_read:
    pushfq
    pop rax
    ret

;rdi = rflags
global rflags_write
rflags_write:
    push rdi
    popfq
    ret
    
global cr4_read
cr4_read:
    mov rax, cr4
    ret

;rdi = value
global cr4_write
cr4_write:
    mov cr4, rdi
    ret
    
global cr3_read
cr3_read:
    mov rax, cr3
    ret

;rdi = value
global cr3_write
cr3_write:
    mov cr3, rdi
    ret
    
global cr2_read
cr2_read:
    mov rax, cr2
    ret

;rdi = value
global cr2_write
cr2_write:
    mov cr2, rdi
    ret
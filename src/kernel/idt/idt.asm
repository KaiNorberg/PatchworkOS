[bits 64]

global idt_load

section .text

;rdi = idt descriptor
idt_load:    
    lidt  [rdi]
    ret
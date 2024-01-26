[bits 64]

global idt_load_descriptor

section .text

;rdi = idt descriptor
idt_load_descriptor:    
    lidt  [rdi]
    ret
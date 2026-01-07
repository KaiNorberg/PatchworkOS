.code64
.intel_syntax noprefix

.global idt_load_descriptor
.type idt_load_descriptor, @function

.text

// rdi = idt descriptor
idt_load_descriptor:
    lidt  [rdi]
    ret

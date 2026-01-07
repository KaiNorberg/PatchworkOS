.code64
.intel_syntax noprefix

#include "kernel/cpu/gdt.h"

.global tss_load
.type tss_load, @function

.text

tss_load:
    mov ax, GDT_TSS
    ltr ax
    ret

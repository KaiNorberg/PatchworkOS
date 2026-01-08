.code64

#include "kernel/cpu/gdt.h"

.global tss_load
.type tss_load, @function

.text

tss_load:
    movw $GDT_TSS, %ax
    ltr %ax
    ret

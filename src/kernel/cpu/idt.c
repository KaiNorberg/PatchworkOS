#include "idt.h"

#include "gdt.h"
#include "interrupt.h"
#include "tss.h"

#include <sys/proc.h>

static idt_t idt ALIGNED(PAGE_SIZE);

extern void idt_load_descriptor(idt_desc_t* descriptor);

static idt_gate_t idt_gate(void* vectorHandler, idt_attributes_t attr, tss_ist_t ist)
{
    idt_gate_t gate;
    gate.offsetLow = (uint64_t)vectorHandler & 0xFFFF;
    gate.codeSegment = GDT_KERNEL_CODE;
    gate.ist = ist;
    gate.attributes = attr;
    gate.offsetMid = ((uint64_t)vectorHandler >> 16) & 0xFFFF;
    gate.offsetHigh = ((uint64_t)vectorHandler >> 32) & 0xFFFFFFFF;
    gate.reserved = 0;
    return gate;
}

void idt_init(void)
{
    idt_attributes_t attr = IDT_ATTR_PRESENT | IDT_ATTR_RING0 | IDT_ATTR_INTERRUPT;

    for (interrupt_t vector = 0; vector < EXCEPTION_AMOUNT; vector++)
    {
        if (vector == EXCEPTION_DOUBLE_FAULT)
        {
            idt.entries[vector] = idt_gate(vectorTable[vector], attr, TSS_IST_DOUBLE_FAULT);
            continue;
        }

        idt.entries[vector] = idt_gate(vectorTable[vector], attr, TSS_IST_EXCEPTION);
    }

    for (interrupt_t vector = EXCEPTION_AMOUNT; vector < INTERRUPT_AMOUNT; vector++)
    {
        idt.entries[vector] = idt_gate(vectorTable[vector], attr, TSS_IST_INTERRUPT);
    }
}

void idt_cpu_load(void)
{
    idt_desc_t idtDesc;
    idtDesc.size = (sizeof(idt_t)) - 1;
    idtDesc.offset = (uint64_t)&idt;
    idt_load_descriptor(&idtDesc);
}

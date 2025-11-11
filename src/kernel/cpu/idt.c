#include <kernel/cpu/idt.h>

#include <kernel/cpu/gdt.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/irq.h>
#include <kernel/cpu/tss.h>

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

    for (irq_virt_t vector = 0; vector < IRQ_VIRT_EXCEPTION_END; vector++)
    {
        if (vector == IRQ_VIRT_DOUBLE_FAULT)
        {
            idt.entries[vector] = idt_gate(vectorTable[vector], attr, TSS_IST_DOUBLE_FAULT);
            continue;
        }

        idt.entries[vector] = idt_gate(vectorTable[vector], attr, TSS_IST_EXCEPTION);
    }

    for (irq_virt_t vector = IRQ_VIRT_EXCEPTION_END; vector < IRQ_VIRT_TOTAL_AMOUNT; vector++)
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

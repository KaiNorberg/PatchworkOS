#include "idt.h"

#include "splash.h"
#include "syscall.h"

#include <sys/proc.h>

ALIGNED(PAGE_SIZE) static idt_t idt;

extern void idt_load_descriptor(idt_desc_t* descriptor);

static void idt_set_vector(uint8_t vector, void* isr, uint8_t privilegeLevel, uint8_t gateType)
{
    idt_entry_t* descriptor = &(idt.entries[vector]);

    descriptor->isrLow = (uint64_t)isr & 0xFFFF;
    descriptor->codeSegment = 0x08;
    descriptor->ist = 0;
    descriptor->attributes = 0b10000000 | (uint8_t)(privilegeLevel << 5) | gateType;
    descriptor->isrMid = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isrHigh = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved = 0;
}

void idt_init(void)
{
    for (uint16_t vector = 0; vector < VECTOR_AMOUNT; vector++)
    {
        idt_set_vector((uint8_t)vector, vectorTable[vector], IDT_RING0, IDT_INTERRUPT_GATE);
    }
    idt_set_vector(SYSCALL_VECTOR, syscall_handler, IDT_RING3, IDT_TRAP_GATE);

    idt_load();
}

void idt_load(void)
{
    idt_desc_t idtDesc;
    idtDesc.size = (sizeof(idt_t)) - 1;
    idtDesc.offset = (uint64_t)&idt;
    idt_load_descriptor(&idtDesc);
}

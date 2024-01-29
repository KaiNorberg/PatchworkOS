#include "idt.h"

#include "io/io.h"
#include "tty/tty.h"

#include "page_allocator/page_allocator.h"
#include "global_heap/global_heap.h"

/*void idt_init() 
{    
    tty_start_message("IDT initializing");

    idt = gmalloc(1);

    for (uint16_t vector = 0; vector < IDT_VECTOR_AMOUNT; vector++) 
    {        
        idt_set_descriptor(vector, interruptVectorTable[vector], IDT_INTERRUPT);
    }
    
    idt_set_descriptor(SYSCALL_VECTOR, interruptVectorTable[SYSCALL_VECTOR], IDT_SYSCALL);

    remap_pic();

    io_outb(PIC1_DATA, 0xFF);
    io_outb(PIC2_DATA, 0xFF);

    //io_pic_clear_mask(IRQ_CASCADE);

    tty_end_message(TTY_MESSAGE_OK);
}*/

void idt_load(Idt* idt)
{
    IdtDesc idtDesc;
    idtDesc.size = (sizeof(Idt)) - 1;
    idtDesc.offset = (uint64_t)idt;
    idt_load_descriptor(&idtDesc);
}

void idt_set_vector(Idt* idt, uint8_t vector, void* isr, uint8_t privilageLevel, uint8_t gateType)
{
    IdtEntry* descriptor = &(idt->entries[vector]);
 
    descriptor->isrLow = (uint64_t)isr & 0xFFFF;
    descriptor->codeSegment = 0x08;
    descriptor->ist = 0;
    descriptor->attributes = 0b10000000 | (privilageLevel << 5) | gateType;
    descriptor->isrMid = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isrHigh = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved = 0;
}
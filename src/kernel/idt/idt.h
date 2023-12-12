#pragma once

#include <stdint.h>

typedef struct {
	uint16_t IsrLow;      // The lower 16 bits of the ISR's address
	uint16_t KernelCS;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t	Ist;          // The IST in the TSS that the CPU will load into RSP; set to zero for now
	uint8_t Attributes;   // Type and attributes; see the IDT page
	uint16_t IsrMid;      // The higher 16 bits of the lower 32 bits of the ISR's address
	uint32_t IsrHigh;     // The higher 32 bits of the ISR's address
	uint32_t Reserved;    // Set to zero
} __attribute__((packed)) IDTEntry;

typedef struct {
	uint16_t Limit;
	uint32_t Base;
} __attribute__((packed)) IDTR;

extern IDTEntry idt[];

void idt_init();

void remap_pic();

void exception_handler();

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);

void enable_irq();

void disable_irq();

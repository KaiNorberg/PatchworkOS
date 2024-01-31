#pragma once

#include "idt/idt.h"
#include "interrupt_frame/interrupt_frame.h"

void master_idt_populate(Idt* idt);

void master_interrupt_handler(InterruptFrame const* interruptFrame);

void master_exception_handler(InterruptFrame const* interruptFrame);

void master_irq_handler(InterruptFrame const* interruptFrame);
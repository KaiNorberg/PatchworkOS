#pragma once

#include "idt/idt.h"
#include "interrupt_frame/interrupt_frame.h"

void master_idt_populate(Idt* idt);

void master_interrupt_handler(InterruptFrame* interruptFrame);

void master_exception_handler(InterruptFrame* interruptFrame);

void master_irq_handler(InterruptFrame* interruptFrame);
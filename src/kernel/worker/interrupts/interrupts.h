#pragma once

#include "idt/idt.h"
#include "page_directory/page_directory.h"
#include "interrupt_frame/interrupt_frame.h"

void worker_idt_init();

Idt* worker_idt();

void worker_interrupt_handler(InterruptFrame* interruptFrame);

void worker_ipi_handler(InterruptFrame* interruptFrame);

void worker_exception_handler(InterruptFrame* interruptFrame);
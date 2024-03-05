#pragma once

#include "page_directory/page_directory.h"
#include "interrupt_frame/interrupt_frame.h"

void interrupt_handler_init();

void interrupt_handler(InterruptFrame* interruptFrame);
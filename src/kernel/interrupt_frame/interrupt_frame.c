#include "interrupt_frame.h"

#include <string.h>

#include "gdt/gdt.h"
#include "heap/heap.h"
#include "registers/registers.h"

void interrupt_frame_copy(InterruptFrame* dest, InterruptFrame const* src)
{
    memcpy(dest, src, sizeof(InterruptFrame));
}
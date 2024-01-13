#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "ram_disk/ram_disk.h"
#include "page_allocator/page_allocator.h"
#include "scheduler/scheduler.h"
#include "program_loader/program_loader.h"
#include "heap/heap.h"
#include "hpet/hpet.h"
#include "kernel/kernel.h"
#include "debug/debug.h"
#include "string/string.h"
#include "queue/queue.h"
#include "smp/smp.h"
#include "global_heap/global_heap.h"
#include "io/io.h"
#include "apic/apic.h"

#include "../common.h"

void main(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    tty_print("\n\r");

    for (uint64_t i = 0; i < 8; i++)
    {
        tty_print("Loading fork_test...\n\r");
        load_program("/bin/fork_test.elf");
    }

    tty_print("Loading test...\n\r");
    load_program("/bin/test.elf");

    tty_print("\n\rKernel Initialized!\n\n\r");

    Ipi ipi = IPI_CREATE(IPI_TYPE_START);
    smp_send_ipi_to_all(ipi);

    while (1)
    {
        asm volatile("hlt");
    }
}
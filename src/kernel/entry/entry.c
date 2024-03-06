#include <common/boot_info/boot_info.h>
#include <stdint.h>

#include "tty/tty.h"
#include "smp/smp.h"
#include "time/time.h"
#include "kernel/kernel.h"
#include "scheduler/scheduler.h"

#include "interrupts/interrupts.h"

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);

    tty_print("\n");
    
    for (uint64_t i = 0; i < 16; i++)
    {
        tty_print("Loading parent...\n");

        scheduler_spawn("ram:/bin/parent.elf");
    }

    tty_print("\nKernel Initialized!\n\n");

    //Temporary for testing
    tty_clear();
    tty_set_row(20);

    Ipi ipi = 
    {
        .type = IPI_TYPE_SCHEDULE
    };
    smp_send_ipi_to_all(ipi);

    while (1)
    {   
        asm volatile("sti");
        asm volatile("hlt");
    }
}

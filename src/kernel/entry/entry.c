#include <common/boot_info/boot_info.h>
#include <stdint.h>

#include "tty/tty.h"
#include "smp/smp.h"
#include "time/time.h"
#include "debug/debug.h"
#include "kernel/kernel.h"
#include "scheduler/scheduler.h"

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);
    
    for (uint64_t i = 0; i < 1000; i++)
    {
        scheduler_spawn("ram:/bin/parent.elf");
    }

    tty_acquire();
    tty_clear();
    tty_set_row(20);
    tty_release();  
 
    //Exit init process
    scheduler_exit(STATUS_SUCCESS);
    debug_panic("Init process returned");
}

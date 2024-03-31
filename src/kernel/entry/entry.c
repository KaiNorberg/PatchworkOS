#include <common/boot_info/boot_info.h>

#include <string.h>

#include "tty/tty.h"
#include "smp/smp.h"
#include "time/time.h"
#include "debug/debug.h"
#include "kernel/kernel.h"
#include "sched/sched.h"
#include "defs/defs.h"
#include "utils/utils.h"
#include "vfs/utils/utils.h"

#include "heap/heap.h"

/*void heap()
{
    for (uint64_t i = 0; i < 5000; i++)
    {
        kmalloc(1000);
    }
}

void pmm()
{
    for (uint64_t i = 0; i < 50000; i++)
    {
        pmm_allocate();
    }
}*/

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);

    /*tty_print("Total: ");
    tty_printi((pmm_total_amount() * PAGE_SIZE) / 1024);
    tty_print("KB\n");

    tty_print("Free: ");
    tty_printi((pmm_free_amount() * PAGE_SIZE) / 1024);
    tty_print("KB\n");

    tty_print("Reserved: ");
    tty_printi((pmm_reserved_amount() * PAGE_SIZE) / 1024);
    tty_print("KB\n");*/

    tty_acquire();
    for (uint64_t i = 0; i < 2; i++)
    {
        sched_spawn("/ram/programs/parent.elf");
    }
    tty_clear();
    tty_set_row(smp_cpu_amount() + 2);
    tty_release();

    //Exit init thread
    sched_thread_exit();
}

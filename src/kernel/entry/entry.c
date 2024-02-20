#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "pmm/pmm.h"
#include "heap/heap.h"
#include "hpet/hpet.h"
#include "kernel/kernel.h"
#include "debug/debug.h"
#include "queue/queue.h"
#include "io/io.h"
#include "apic/apic.h"
#include "vector/vector.h"
#include "list/list.h"
#include "vfs/vfs.h"

#include "master/master.h"

#include "worker_pool/worker_pool.h"

#include <common/boot_info/boot_info.h>
#include <libc/string.h>

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);

    tty_print("\n");

#if 1
    for (uint64_t i = 0; i < 2; i++)
    {
        tty_print("Loading parent...\n");    
        
        worker_pool_spawn("ram:/bin/parent.elf");
    }
#else
    for (uint64_t i = 0; i < 4; i++)
    {
        tty_print("Loading sleep_test...\n");
        
        worker_pool_spawn("ram:/bin/sleep_test.elf");
    }
#endif

    tty_print("\n\rKernel Initialized!\n\n");

    //Temporary for testing
    tty_clear();
    tty_set_row(20);

    master_entry();

    while (1)
    {
        asm volatile("hlt");
    }
}
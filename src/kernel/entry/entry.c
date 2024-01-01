#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "file_system/file_system.h"
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

#include "../common.h"

void main(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    tty_print("\n\r");

    tty_print("Loading fork_test...\n\r");
    load_program("/programs/fork_test/fork_test.elf");

    tty_print("Loading test...\n\r");
    load_program("/programs/test/test.elf");

    tty_print("\n\r");                                                                                      

    tty_print("Yielding to user space...\n\r");
    
    tty_print("\n\r");

    interrupts_enable();

    scheduler_yield_to_user_space((void*)tss_get(smp_current_cpu()->id)->rsp0);

    tty_print("\nBack in the main task, if you see this something has gone very wrong!\n\n\r");

    while (1)
    {
        asm volatile("hlt");
    }
}
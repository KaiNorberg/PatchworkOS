#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "ram_disk/ram_disk.h"
#include "page_allocator/page_allocator.h"
#include "program_loader/program_loader.h"
#include "heap/heap.h"
#include "hpet/hpet.h"
#include "kernel/kernel.h"
#include "debug/debug.h"
#include "string/string.h"
#include "queue/queue.h"
#include "global_heap/global_heap.h"
#include "io/io.h"
#include "apic/apic.h"
#include "vector/vector.h"
#include "master/master.h"

#include "../common.h"

void main(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    tty_print("\n\r");

/*#if 1
    for (uint64_t i = 0; i < 4; i++)
    {
        tty_print("Loading fork_test...\n\r");
        load_program("/bin/fork_test.elf");
    }
#else
    for (uint64_t i = 0; i < 2; i++)
    {
        tty_print("Loading sleep_test...\n\r");
        load_program("/bin/sleep_test.elf");
    }
#endif*/

    tty_print("\n\rKernel Initialized!\n\n\r");

    //Temporary for testing
    //tty_clear();
    tty_set_cursor_pos(0, 16 * 20);

    master_entry();

    while (1)
    {
        asm volatile("hlt");
    }
}
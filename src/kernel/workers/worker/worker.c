#include "worker.h"

#include "page_directory/page_directory.h"
#include "madt/madt.h"
#include "tty/tty.h"
#include "utils/utils.h"
#include "page_allocator/page_allocator.h"
#include "string/string.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "master/master.h"
#include "gdt/gdt.h"
#include "idt/idt.h"

#include "workers/workers.h"

void worker_entry()
{
    Worker* worker = worker_self_brute();
    write_msr(MSR_WORKER_ID, worker->id);
    
    gdt_load();
    idt_load(worker_idt_get());

    tty_print("Hello from worker "); tty_printx(worker->id); tty_print("! ");

    local_apic_init();

    worker->running = 1;

    apic_timer_init(2);

    while (1)
    {
        asm volatile("sti");
        asm volatile("hlt");
    }
}

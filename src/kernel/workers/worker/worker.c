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
#include "workers/workers.h"

void worker_entry()
{
    Worker* worker = worker_self_brute();
    write_msr(MSR_WORKER_ID, worker->id);

    tty_print("Hello from worker "); tty_printx(worker->id); tty_print("! ");

    /*idt_load();
    gdt_load();
    //gdt_load_tss(tss_get(smp_current_cpu()->id));

    local_apic_init();

    interrupts_enable();*/

    worker->running = 1;

    while (1)
    {
        asm volatile("hlt");
    }
}

#include "startup.h"

#include "page_directory/page_directory.h"
#include "madt/madt.h"
#include "tty/tty.h"
#include "utils/utils.h"
#include "pmm/pmm.h"
#include "apic/apic.h"
#include "hpet/hpet.h"
#include "vmm/vmm.h"

#include "master/master.h"

#include <libc/string.h>

void* worker_trampoline_setup()
{
    void* oldData = pmm_allocate();
    memcpy(oldData, WORKER_TRAMPOLINE_LOADED_START, WORKER_TRAMPOLINE_SIZE);

    memcpy(WORKER_TRAMPOLINE_LOADED_START, worker_trampoline_start, WORKER_TRAMPOLINE_SIZE);

    WRITE_64(WORKER_TRAMPOLINE_PAGE_DIRECTORY_ADDRESS, (uint64_t)vmm_kernel_directory());
    WRITE_64(WORKER_TRAMPOLINE_ENTRY_ADDRESS, worker_entry);

    return oldData;
}

void worker_trampoline_cleanup(void* oldData)
{    
    memcpy(WORKER_TRAMPOLINE_LOADED_START, oldData, WORKER_TRAMPOLINE_SIZE);
    pmm_free_page(oldData);
}

uint8_t worker_push(Worker workers[], uint8_t id, LocalApicRecord const* record)
{
    workers[id].present = 1;
    workers[id].running = 0;
    workers[id].id = id;
    workers[id].apicId = record->localApicId;

    workers[id].tss = tss_new();
    workers[id].ipi = (Ipi){.type = IPI_WORKER_NONE};
    workers[id].scheduler = scheduler_new();

    WRITE_64(WORKER_TRAMPOLINE_STACK_TOP_ADDRESS, (void*)workers[id].tss->rsp0);

    local_apic_send_init(record->localApicId);
    hpet_sleep(10);
    local_apic_send_sipi(record->localApicId, ((uint64_t)WORKER_TRAMPOLINE_LOADED_START) / PAGE_SIZE);

    uint64_t timeout = 1000;
    while (!workers[id].running) 
    {
        hpet_sleep(1);
        timeout--;
        if (timeout == 0)
        {
            return 0;
        }
    }

    return 1;
}

void workers_startup(Worker workers[], uint8_t* workerAmount)
{
    memset(workers, 0, sizeof(Worker) * MAX_WORKER_AMOUNT);
    *workerAmount = 0;

    void* oldData = worker_trampoline_setup();

    LocalApicRecord* record = madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
    while (record != 0)
    {
        if (LOCAL_APIC_RECORD_IS_ENABLEABLE(record) && 
            record->localApicId != master_apic_id())
        {                
            if (!worker_push(workers, *workerAmount, record))
            {    
                tty_print("Worker "); tty_printi(record->cpuId); tty_print(" failed to start!");
                tty_end_message(TTY_MESSAGE_ER);
            }
            (*workerAmount)++;
        }

        record = madt_next_record(record, MADT_RECORD_TYPE_LOCAL_APIC);
    }

    worker_trampoline_cleanup(oldData);
}
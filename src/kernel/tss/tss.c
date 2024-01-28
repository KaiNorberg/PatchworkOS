#include "tss.h"

#include "string/string.h"
#include "process/process.h"
#include "global_heap/global_heap.h"
#include "tty/tty.h"
#include "page_allocator/page_allocator.h"
#include "debug/debug.h"

Tss* tssArray;

void tss_init()
{    
    tty_start_message("TSS loading");

    /*uint64_t tssArrayPageAmount = GET_SIZE_IN_PAGES(SMP_MAX_CPU_AMOUNT * sizeof(Tss));

    tssArray = gmalloc(tssArrayPageAmount);
    memclr(tssArray, tssArrayPageAmount * 0x1000);*/

    tty_end_message(TTY_MESSAGE_OK);
}

Tss* tss_get(uint8_t cpuId)
{
    Tss* tss = &tssArray[cpuId];

    if (tss->iopb == 0)
    {
        tss->rsp0 = (uint64_t)gmalloc(1) + 0x1000;
        tss->rsp1 = tss->rsp0;
        tss->rsp2 = tss->rsp0;
        tss->iopb = sizeof(Tss);
    }

    return tss;
}

void* tss_kernel_stack()
{
    return 0;
    //return (void*)(tss_get(smp_current_cpu()->id)->rsp0);
}
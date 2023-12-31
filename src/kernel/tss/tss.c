#include "tss.h"

#include "string/string.h"
#include "process/process.h"
#include "global_heap/global_heap.h"
#include "tty/tty.h"
#include "smp/smp.h"
#include "page_allocator/page_allocator.h"
#include "debug/debug.h"

Tss* tssArray;

void tss_init()
{    
    tty_start_message("TSS loading");

    uint64_t tssArrayPageAmount = GET_SIZE_IN_PAGES(SMP_MAX_CPU_AMOUNT * sizeof(Tss));

    tssArray = gmalloc(tssArrayPageAmount, PAGE_DIR_READ_WRITE);
    memset(tssArray, 0, tssArrayPageAmount * 0x1000);

    tty_end_message(TTY_MESSAGE_OK);
}

Tss* tss_get(uint8_t cpuId)
{
    if (cpuId >= SMP_MAX_CPU_AMOUNT)
    {
        debug_panic("Unable to retrive TSS");
    }

    Tss* tss = &tssArray[cpuId];

    if (tss->iopb == 0)
    {
        tss->rsp0 = (uint64_t)gmalloc(1, PAGE_DIR_READ_WRITE) + 0x1000;
        tss->rsp1 = tss->rsp0;
        tss->rsp2 = tss->rsp0;
        tss->iopb = sizeof(Tss);
    }

    return tss;
}
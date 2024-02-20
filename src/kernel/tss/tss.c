#include "tss.h"

#include "vmm/vmm.h"
#include "heap/heap.h"

Tss* tss_new()
{
    Tss* tss = kmalloc(sizeof(Tss));

    tss->rsp0 = (uint64_t)vmm_request_memory(1, PAGE_FLAG_READ_WRITE) + 0xFFF;
    tss->rsp1 = tss->rsp0;
    tss->rsp2 = tss->rsp0;
    tss->iopb = sizeof(Tss);

    return tss;
}
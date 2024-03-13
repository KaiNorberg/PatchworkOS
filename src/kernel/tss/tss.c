#include "tss.h"

#include "vmm/vmm.h"
#include "heap/heap.h"
#include "page_directory/page_directory.h"

Tss* tss_new(void)
{
    Tss* tss = kmalloc(sizeof(Tss));

    tss->rsp0 = 0;
    tss->rsp1 = 0;
    tss->rsp2 = 0;
    tss->iopb = sizeof(Tss);

    return tss;
}
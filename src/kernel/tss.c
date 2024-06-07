#include "tss.h"

void tss_init(Tss* tss)
{
    tss->rsp0 = 0;
    tss->rsp1 = 0;
    tss->rsp2 = 0;
    tss->iopb = sizeof(Tss);
}

void tss_stack_load(Tss* tss, void* stackTop)
{
    tss->rsp0 = (uint64_t)stackTop;
    tss->rsp1 = (uint64_t)stackTop;
    tss->rsp2 = (uint64_t)stackTop;
}
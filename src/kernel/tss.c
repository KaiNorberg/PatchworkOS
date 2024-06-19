#include "tss.h"

void tss_init(tss_t* tss)
{
    tss->rsp0 = 0;
    tss->rsp1 = 0;
    tss->rsp2 = 0;
    tss->iopb = sizeof(tss_t);
}

void tss_stack_load(tss_t* tss, void* stackTop)
{
    tss->rsp0 = (uint64_t)stackTop;
    tss->rsp1 = (uint64_t)stackTop;
    tss->rsp2 = (uint64_t)stackTop;
}

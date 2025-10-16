#include "tss.h"

#include <assert.h>

void tss_init(tss_t* tss)
{
    tss->rsp0 = 0;
    tss->rsp1 = 0;
    tss->rsp2 = 0;
    tss->iopb = sizeof(tss_t);
}

void tss_kernel_stack_load(tss_t* tss, stack_pointer_t* stack)
{
    tss->rsp0 = stack->top;
    tss->rsp1 = stack->top;
    tss->rsp2 = stack->top;
}

void tss_ist_load(tss_t* tss, tss_ist_t ist, stack_pointer_t* stack)
{
    assert(ist < TSS_IST_COUNT && ist >= TSS_IST1);
    tss->ist[ist - 1] = stack->top;
}

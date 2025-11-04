#include <kernel/cpu/tss.h>

#include <assert.h>

void tss_init(tss_t* tss)
{
    tss->reserved1 = 0;
    tss->rsp0 = 0;
    tss->rsp1 = 0;
    tss->rsp2 = 0;
    tss->reserved2 = 0;
    for (int i = 0; i < TSS_IST_COUNT; i++)
    {
        tss->ist[i] = 0;
    }
    tss->reserved3 = 0;
    tss->reserved4 = 0;
    tss->iopb = sizeof(tss_t);
}

void tss_ist_load(tss_t* tss, tss_ist_t ist, stack_pointer_t* stack)
{
    assert(ist < TSS_IST_COUNT && ist >= TSS_IST1);
    tss->ist[ist - 1] = stack->top;
}

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libc/io.h>
#include <libc/proc.h>
#include <libc/status.h>
#include <threads.h>
#include <time.h>

#define SENTRIES 64
#define CENTRIES 128

int main()
{
    printf("setting up ring test...\n");
    ioring_t ring;
    status_t status = ioring_setup(&ring, NULL, SENTRIES, CENTRIES);
    if (IS_ERR(status))
    {
        printf("failed to set up ring (%s, %s)\n", status_src_str(STATUS_SRC(status)), status_code_str(STATUS_CODE(status)));
        ;
        return errno;
    }

    memset(&ring.ctrl->regs, -1, sizeof(ring.ctrl->regs));

    printf("pushing nop iosqe to ring %llu...\n", ring.id);
    iosqe_t* iosqe = iosqe_get(&ring);
    ioprep_nop(iosqe, IOSQE_HARDLINK, CLOCKS_PER_SEC, 0x1234);
    iosqe_put(&ring);

    printf("pushing nop iosqe to ring %llu...\n", ring.id);
    iosqe = iosqe_get(&ring);
    ioprep_nop(iosqe, IOSQE_NORMAL, CLOCKS_PER_SEC, 0x5678);
    iosqe_put(&ring);

    printf("entering ring...\n");
    status = ioring_enter(&ring, 2, 0, NULL);
    if (IS_ERR(status))
    {
        printf("failed to enter ring (%s, %s)\n", status_src_str(STATUS_SRC(status)), status_code_str(STATUS_CODE(status)));
        return errno;
    }

    printf("pushing cancel iosqe to ring %llu...\n", ring.id);
    iosqe = iosqe_get(&ring);
    ioprep_cancel(iosqe, IOSQE_NORMAL, CLOCKS_NEVER, 0x9012, 0x1234, IOCANCEL_ALL);
    iosqe_put(&ring);

    printf("entering ring to submit cancel iosqe...\n");
    status = ioring_enter(&ring, 1, 0, NULL);
    if (IS_ERR(status))
    {
        printf("failed to enter ring (%s, %s)\n", status_src_str(STATUS_SRC(status)), status_code_str(STATUS_CODE(status)));
        return errno;
    }

    printf("sleeping for 5 seconds...\n");
    struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
    thrd_sleep(&ts, NULL);

    iocqe_t* iocqe;
    while ((iocqe = iocqe_get(&ring)) != NULL)
    {
        printf("iocqe:\n");

        printf("iocqe data: %p\n", iocqe->data);
        printf("iocqe op: %d\n", iocqe->op);
        printf("iocqe status%Y\n", status);
        printf("iocqe result: %llu\n", iocqe->result);

        iocqe_put(&ring);
    }

    printf("registers:\n");
    for (uint64_t i = 0; i < IOSQE_REGS_MAX; i++)
    {
        printf("reg[%llu]: %llu\n", i, ring.ctrl->regs[i]);
    }

    printf("tearing down ring...\n");
    ioring_teardown(&ring);
    return 0;
}

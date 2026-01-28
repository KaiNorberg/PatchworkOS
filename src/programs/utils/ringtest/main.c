#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioring.h>
#include <sys/proc.h>
#include <sys/status.h>

#define SENTRIES 64
#define CENTRIES 128

int main()
{
    printf("setting up ring test...\n");
    ioring_t ring;
    status_t status = ioring_setup(&ring, NULL, SENTRIES, CENTRIES);
    if (IS_ERR(status))
    {
        printf("failed to set up ring (%s, %s)\n", srctostr(ST_SRC(status)), codetostr(ST_CODE(status)));;
        return errno;
    }

    memset(&ring.ctrl->regs, -1, sizeof(ring.ctrl->regs));

    printf("pushing nop sqe to ring %llu...\n", ring.id);
    sqe_t* sqe = sqe_get(&ring);
    sqe_prep_nop(sqe, SQE_HARDLINK, CLOCKS_PER_SEC, 0x1234);
    sqe_put(&ring);

    printf("pushing nop sqe to ring %llu...\n", ring.id);
    sqe = sqe_get(&ring);
    sqe_prep_nop(sqe, SQE_NORMAL, CLOCKS_PER_SEC, 0x5678);
    sqe_put(&ring);

    printf("entering ring...\n");
    status = ioring_enter(&ring, 2, 0, NULL);
    if (IS_ERR(status))
    {
        printf("failed to enter ring (%s, %s)\n", srctostr(ST_SRC(status)), codetostr(ST_CODE(status)));
        return errno;
    }

    printf("pushing cancel sqe to ring %llu...\n", ring.id);
    sqe = sqe_get(&ring);
    sqe_prep_cancel(sqe, SQE_NORMAL, CLOCKS_NEVER, 0x9012, 0x1234, IO_CANCEL_ALL);
    sqe_put(&ring);

    printf("entering ring to submit cancel sqe...\n");
    status = ioring_enter(&ring, 1, 0, NULL);
    if (IS_ERR(status))
    {
        printf("failed to enter ring (%s, %s)\n", srctostr(ST_SRC(status)), codetostr(ST_CODE(status)));
        return errno;
    }

    printf("sleeping for 5 seconds...\n");
    nanosleep(CLOCKS_PER_SEC * 5);

    cqe_t* cqe;
    while ((cqe = cqe_get(&ring)) != NULL)
    {
        printf("cqe:\n");

        printf("cqe data: %p\n", cqe->data);
        printf("cqe op: %d\n", cqe->op);
        printf("cqe status: %s, %s\n", srctostr(ST_SRC(cqe->status)), codetostr(ST_CODE(cqe->status)));
        printf("cqe result: %llu\n", cqe->_result);

        cqe_put(&ring);
    }

    printf("registers:\n");
    for (uint64_t i = 0; i < SQE_REGS_MAX; i++)
    {
        printf("reg[%llu]: %llu\n", i, ring.ctrl->regs[i]);
    }

    printf("tearing down ring...\n");
    ioring_teardown(&ring);
    return 0;
}

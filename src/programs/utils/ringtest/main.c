#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioring.h>
#include <sys/proc.h>

#define SENTRIES 64
#define CENTRIES 128

int main()
{
    printf("setting up ring test...\n");
    ioring_t ring;
    ioring_id_t id = ioring_setup(&ring, NULL, SENTRIES, CENTRIES);
    if (id == _FAIL)
    {
        printf("failed to set up ring\n");
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
    if (ioring_enter(id, 2, 0) == _FAIL)
    {
        printf("failed to enter ring\n");
        return errno;
    }

    printf("pushing cancel sqe to ring %llu...\n", ring.id);
    sqe = sqe_get(&ring);
    sqe_prep_cancel(sqe, SQE_NORMAL, CLOCKS_NEVER, 0x9012, 0x1234, IO_CANCEL_ALL);
    sqe_put(&ring);

    printf("entering ring to submit cancel sqe...\n");
    if (ioring_enter(id, 1, 0) == _FAIL)
    {
        printf("failed to enter ring\n");
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
        printf("cqe error: %s\n", strerror(cqe->error));
        printf("cqe result: %llu\n", cqe->_result);

        cqe_put(&ring);
    }

    printf("registers:\n");
    for (uint64_t i = 0; i < SQE_REGS_MAX; i++)
    {
        printf("reg[%llu]: %llu\n", i, ring.ctrl->regs[i]);
    }

    printf("tearing down ring...\n");
    ioring_teardown(id);
    return 0;
}

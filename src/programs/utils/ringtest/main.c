#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/uring.h>

#define SENTRIES 64
#define CENTRIES 128

int main()
{
    printf("setting up ring test...\n");
    ring_t ring;
    ring_id_t id = setup(&ring, NULL, SENTRIES, CENTRIES);
    if (id == ERR)
    {
        printf("failed to set up ring\n");
        return errno;
    }

    memset(&ring.ctrl->regs, -1, sizeof(ring.ctrl->regs));

    printf("pushing nop sqe to ring %llu...\n", id);
    sqe_t sqe = SQE_CREATE(VERB_NOP, SQE_HARDLINK | (SQE_REG0 << SQE_SAVE), CLOCKS_PER_SEC, 0x1234);
    sqe_push(&ring, &sqe);

    printf("pushing nop sqe to ring %llu...\n", id);
    sqe = (sqe_t)SQE_CREATE(VERB_NOP, SQE_LINK, CLOCKS_PER_SEC, 0x5678);
    sqe_push(&ring, &sqe);

    printf("entering ring...\n");
    if (enter(id, 2, 2) == ERR)
    {
        printf("failed to enter ring\n");
        return errno;
    }

    cqe_t cqe;
    while (cqe_pop(&ring, &cqe))
    {
        printf("cqe:\n");

        printf("cqe data: %p\n", cqe.data);
        printf("cqe verb: %d\n", cqe.verb);
        printf("cqe error: %s\n", strerror(cqe.error));
        printf("cqe result: %llu\n", cqe._result);
    }

    printf("registers:\n");
    for (uint64_t i = 0; i < SQE_REGS_MAX; i++)
    {
        printf("reg[%llu]: %llu\n", i, ring.ctrl->regs[i]);
    }

    printf("tearing down ring...\n");
    teardown(id);
    return 0;
}

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/rings.h>

#define SENTRIES 64
#define CENTRIES 128

int main()
{
    printf("setting up rings test...\n");
    rings_t rings;
    rings_id_t id = setup(&rings, NULL, SENTRIES, CENTRIES);
    if (id == ERR)
    {
        printf("failed to set up rings\n");
        return errno;
    }

    memset(&rings.shared->regs, -1, sizeof(rings.shared->regs));

    printf("pushing nop sqe to rings %llu...\n", id);
    sqe_t sqe = SQE_CREATE(RINGS_NOP, SQE_LINK | (SQE_REG0 << SQE_SAVE), CLOCKS_PER_SEC, 0x1234);
    sqe_push(&rings, &sqe);

    printf("pushing nop sqe to rings %llu...\n", id);
    sqe = (sqe_t)SQE_CREATE(RINGS_NOP, SQE_LINK, CLOCKS_PER_SEC, 0x5678);
    sqe_push(&rings, &sqe);

    printf("entering rings...\n");
    if (enter(id, 2, 2) == ERR)
    {
        printf("failed to enter rings\n");
        return errno;
    }

    cqe_t cqe;
    while (cqe_pop(&rings, &cqe))
    {
        printf("cqe:\n");

        printf("cqe data: %p\n", cqe.data);
        printf("cqe opcode: %d\n", cqe.opcode);
        printf("cqe error: %s\n", strerror(cqe.error));
        printf("cqe result: %llu\n", cqe._raw);
    }

    printf("registers:\n");
    for (uint64_t i = 0; i < SEQ_REGS_MAX; i++)
    {
        printf("reg[%llu]: %llu\n", i, rings.shared->regs[i]);
    }

    printf("tearing down rings...\n");
    teardown(id);
    return 0;
}

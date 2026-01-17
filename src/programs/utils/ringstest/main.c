#include <errno.h>
#include <stdio.h>
#include <sys/rings.h>

#define SENTRIES 64
#define CENTRIES 64

int main()
{
    printf("setting up rings test...\n");
    rings_t rings;
    setup(&rings, NULL, SENTRIES, CENTRIES);

    printf("pushing nop sqe...\n");
    sqe_t sqe = SQE_CREATE(RINGS_NOP, SQE_REG1 << SQE_SAVE, CLOCKS_PER_SEC, 0x1234);
    sqe_push(&rings, &sqe);

    sqe = (sqe_t)SQE_CREATE(RINGS_NOP, SQE_REG1 << SQE_LOAD0, CLOCKS_PER_SEC, 0x5678);
    sqe_push(&rings, &sqe);

    printf("entering rings...\n");
    enter(1, 1);

    printf("popping cqe...\n");
    cqe_t cqe;
    cqe_pop(&rings, &cqe);
    if (cqe.error != EOK)
    {
        printf("cqe returned error: %d\n", cqe.error);
        return 1;
    }

    printf("cqe data: %p\n", cqe.data);
    printf("cqe opcode: %d\n", cqe.opcode);
    printf("cqe error: %d\n", cqe.error);

    printf("tearing down rings...\n");
    teardown();
    return 0;
}

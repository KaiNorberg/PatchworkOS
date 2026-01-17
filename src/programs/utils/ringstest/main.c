#include <errno.h>
#include <stdio.h>
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

    printf("pushing nop sqe to rings %llu...\n", id);
    sqe_t sqe = SQE_CREATE(RINGS_NOP, SQE_LINK, CLOCKS_PER_SEC, 0x1234);
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
        printf("popped cqe...\n");
        if (cqe.error != EOK)
        {
            printf("cqe returned error\n");
            return cqe.error;
        }

        printf("cqe data: %p\n", cqe.data);
        printf("cqe opcode: %d\n", cqe.opcode);
        printf("cqe error: %d\n", cqe.error);
    }

    printf("tearing down rings...\n");
    teardown(id);
    return 0;
}

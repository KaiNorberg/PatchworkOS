#include <stdlib.h>
#include <sys/ioring.h>
#include <user/common/io.h>

void iosync_many(iosqe_t* sqes, iocqe_t* cqes, size_t count, size_t wait, size_t* completed)
{
    mtx_lock(&_stdIoringMtx);

    size_t submitted = 0;
    for (size_t i = 0; i < count; i++)
    {
        iosqe_t* sqe;
        while ((sqe = iosqe_get(&_stdIoring)) == NULL)
        {
            if (submitted > 0)
            {
                ioring_enter(&_stdIoring, submitted, 0, NULL);
                submitted = 0;
            }
        }
        *sqe = sqes[i];
        iosqe_put(&_stdIoring);
        submitted++;
    }

    size_t processed;
    ioring_enter(&_stdIoring, submitted, wait, &processed);

    size_t cqeCount = 0;
    while (cqeCount < count)
    {
        iocqe_t* cqe = iocqe_get(&_stdIoring);
        if (cqe == NULL)
        {
            if (cqeCount < wait)
            {
                ioring_enter(&_stdIoring, 0, wait - cqeCount, &processed);
                continue;
            }
            break;
        }
        if (cqes != NULL)
        {
            cqes[cqeCount] = *cqe;
        }
        iocqe_put(&_stdIoring);
        cqeCount++;
    }

    if (completed != NULL)
    {
        *completed = cqeCount;
    }

    mtx_unlock(&_stdIoringMtx);
}
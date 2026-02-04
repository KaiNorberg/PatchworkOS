#include <sys/ioring.h>
#include <stdlib.h>

#include "user/common/io.h"

void iosync(iosqe_t* sqe, iocqe_t* cqe)
{
    iosync_many(sqe, cqe, 1, 1, NULL);
}
#include "user/common/io.h"
#include <stdlib.h>
#include <sys/io.h>
#include <sys/status.h>

status_t iopolln(iopoll_t* fds, size_t nfds, clock_t timeout, size_t* count)
{
    if (nfds == 0)
    {
        if (count != NULL)
        {
            *count = 0;
        }
        return OK;
    }

    iosqe_t* sqes = malloc(sizeof(iosqe_t) * nfds);
    if (sqes == NULL)
    {
        return ERR(LIBSTD, NOMEM);
    }

    iocqe_t* cqes = malloc(sizeof(iocqe_t) * nfds);
    if (cqes == NULL)
    {
        free(sqes);
        return ERR(LIBSTD, NOMEM);
    }

    for (size_t i = 0; i < nfds; i++)
    {
        fds[i].revents = 0;
        ioprep_poll(&sqes[i], IOSQE_NORMAL, timeout, (uintptr_t)&fds[i], fds[i].fd, fds[i].events);
    }

    size_t completed;
    size_t wait = (timeout == 0) ? 0 : 1;
    iosyncn(sqes, cqes, nfds, wait, &completed);

    size_t events = 0;
    for (size_t i = 0; i < completed; i++)
    {
        iopoll_t* poll = (iopoll_t*)cqes[i].data;

        if (poll < fds || poll >= fds + nfds)
        {
            continue;
        }

        size_t index = poll - fds;
        sqes[index].data = 0;

        if (IS_INFO(cqes[i].status))
        {
            poll->revents = (iopoll_t)cqes[i].result;
            if (poll->revents != 0)
            {
                events++;
            }
        }
        else if (IS_CODE(cqes[i].status, TIMEOUT))
        {
            // Timeout
        }
        else
        {
            poll->revents = IOEVENT_ERROR;
            events++;
        }
    }

    size_t cancels = 0;
    for (size_t i = 0; i < nfds; i++)
    {
        if (sqes[i].data != 0)
        {
            ioprep_cancel(&sqes[cancels], IOSQE_NORMAL, CLOCKS_NEVER, 0, (uintptr_t)&fds[i], IOCANCEL_ALL);
            cancels++;
        }
    }

    if (cancels > 0)
    {
        iosyncn(sqes, NULL, cancels, cancels, NULL);

        mtx_lock(&_stdIoringMtx);
        size_t drained = 0;
        while (drained < cancels)
        {
            iocqe_t* cqe = iocqe_get(&_stdIoring);
            if (cqe != NULL)
            {
                iocqe_put(&_stdIoring);
                drained++;
            }
            else
            {
                ioring_enter(&_stdIoring, 0, cancels - drained, NULL);
            }
        }
        mtx_unlock(&_stdIoringMtx);
    }

    free(sqes);
    free(cqes);

    if (count != NULL)
    {
        *count = events;
    }

    return OK;
}

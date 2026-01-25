#include <kernel/fs/ctl.h>

#include <kernel/mem/pmm.h>
#include <kernel/sched/thread.h>

#include <errno.h>
#include <sys/argsplit.h>

static uint64_t ctl_dispatch_one(ctl_t* ctls, file_t* file, uint64_t argc, const char** argv)
{
    if (ctls == NULL || file == NULL || argv == NULL || argc == 0)
    {
        errno = EINVAL;
        return _FAIL;
    }

    ctl_t* ctl = &ctls[0];
    while (ctl->name != NULL)
    {
        if (strcmp(ctl->name, argv[0]) == 0)
        {
            if (argc < ctl->argcMin || argc > ctl->argcMax)
            {
                errno = EINVAL;
                return _FAIL;
            }

            if (ctl->func(file, argc, argv) == _FAIL)
            {
                return _FAIL;
            }
            return 0;
        }

        ctl++;
    }

    errno = ENOENT;
    return _FAIL;
}

uint64_t ctl_dispatch(ctl_t* ctls, file_t* file, const void* buffer, size_t count)
{
    if (ctls == NULL || file == NULL || buffer == NULL || count == 0)
    {
        errno = EINVAL;
        return _FAIL;
    }

    uint8_t argBuffer[CTL_MAX_BUFFER];

    uint64_t argc;
    const char** argv = argsplit_buf(argBuffer, CTL_MAX_BUFFER, buffer, count, &argc);
    if (argv == NULL)
    {
        errno = E2BIG;
        return _FAIL;
    }
    if (argc == 0)
    {
        errno = ENOENT;
        return _FAIL;
    }

    for (uint64_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "&&") == 0)
        {
            uint64_t res = ctl_dispatch_one(ctls, file, i, argv);
            if (res == _FAIL)
            {
                return _FAIL;
            }

            argc -= (i + 1);
            argv += (i + 1);
            i = -1;
        }
    }

    if (ctl_dispatch_one(ctls, file, argc, argv) == _FAIL)
    {
        return _FAIL;
    }
    return count;
}

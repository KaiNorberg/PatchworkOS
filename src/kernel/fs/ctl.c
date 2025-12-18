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
        return ERR;
    }

    ctl_t* ctl = &ctls[0];
    while (ctl->name != NULL)
    {
        if (strcmp(ctl->name, argv[0]) == 0)
        {
            if (argc < ctl->argcMin || argc > ctl->argcMax)
            {
                errno = EINVAL;
                return ERR;
            }

            if (ctl->func(file, argc, argv) == ERR)
            {
                return ERR;
            }
            return 0;
        }

        ctl++;
    }

    errno = ENOENT;
    return ERR;
}

uint64_t ctl_dispatch(ctl_t* ctls, file_t* file, const void* buffer, uint64_t count)
{
    if (ctls == NULL || file == NULL || buffer == NULL || count == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (count > MAX_PATH)
    {
        errno = E2BIG;
        return ERR;
    }

    uint8_t argBuffer[MAX_PATH];

    uint64_t argc;
    const char** argv = argsplit_buf(argBuffer, MAX_PATH, buffer, count, &argc);
    if (argv == NULL)
    {
        return ERR;
    }
    if (argc == 0)
    {
        errno = ENOENT;
        return ERR;
    }

    for (uint64_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "&&") == 0)
        {
            uint64_t res = ctl_dispatch_one(ctls, file, i, argv);
            if (res == ERR)
            {
                return ERR;
            }

            argc -= (i + 1);
            argv += (i + 1);
            i = -1;
        }
    }

    if (ctl_dispatch_one(ctls, file, argc, argv) == ERR)
    {
        return ERR;
    }
    return count;
}

#include <kernel/fs/ctl.h>

#include <kernel/mem/pmm.h>
#include <kernel/sched/thread.h>

#include <sys/argsplit.h>

static status_t ctl_dispatch_one(ctl_t* ctls, file_t* file, uint64_t argc, const char** argv)
{
    if (ctls == NULL || file == NULL || argv == NULL || argc == 0)
    {
        return ERR(FS, INVAL);
    }

    ctl_t* ctl = &ctls[0];
    while (ctl->name != NULL)
    {
        if (strcmp(ctl->name, argv[0]) == 0)
        {
            if (argc < ctl->argcMin || argc > ctl->argcMax)
            {
                return ERR(FS, ARGC);
            }

            return ctl->func(file, argc, argv);
        }

        ctl++;
    }

    return ERR(FS, INVALCTL);
}

status_t ctl_dispatch(ctl_t* ctls, file_t* file, const void* buffer, size_t count)
{
    if (ctls == NULL || file == NULL || buffer == NULL || count == 0)
    {
        return ERR(FS, INVAL);
    }

    uint8_t argBuffer[CTL_MAX_BUFFER];

    uint64_t argc;
    const char** argv = argsplit_buf(argBuffer, CTL_MAX_BUFFER, buffer, count, &argc);
    if (argv == NULL)
    {
        return ERR(FS, TOOBIG);
    }
    if (argc == 0)
    {
        return ERR(FS, ARGC);
    }

    for (uint64_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "&&") == 0)
        {
            status_t res = ctl_dispatch_one(ctls, file, i, argv);
            if (IS_ERR(res))
            {
                return res;
            }

            argc -= (i + 1);
            argv += (i + 1);
            i = -1;
        }
    }

    return ctl_dispatch_one(ctls, file, argc, argv);
}

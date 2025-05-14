#include "ctl.h"

#include "defs.h"
#include "pmm.h"
#include "sched.h"

#include <errno.h>
#include <stdio.h>
#include <sys/argsplit.h>

uint64_t ctl_dispatch(ctl_t* ctls, file_t* file, const void* buffer, uint64_t count)
{
    if (count == 0)
    {
        return 0;
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
        return ERROR(EREQ);
    }

    ctl_t* ctl = &ctls[0];
    while (ctl->name != NULL)
    {
        if (strcmp(ctl->name, argv[0]) == 0)
        {
            if (ctl->argcMin < argc || ctl->argcMax > argc)
            {
                return ERROR(EREQ);
            }

            if (ctl->func(file, argc, argv) == ERR)
            {
                return ERR;
            }
            return count;
        }

        ctl++;
    }

    return ERROR(EREQ);
}

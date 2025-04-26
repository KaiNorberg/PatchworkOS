#include "actions.h"

#include "defs.h"
#include "pmm.h"
#include "sched.h"

#include <errno.h>
#include <stdio.h>
#include <sys/argsplit.h>

uint64_t actions_dispatch(actions_t* actions, const void* buffer, uint64_t count, void* private)
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

    action_t* action = *actions;
    while (action->name != NULL)
    {
        if (strcmp(action->name, argv[0]) == 0)
        {
            if (action->argcMin < argc || action->argcMax > argc)
            {
                return ERROR(EREQ);
            }

            if (action->func(argc, argv, private) == ERR)
            {
                return ERR;
            }
            return count;
        }

        action++;
    }

    return ERROR(EREQ);
}

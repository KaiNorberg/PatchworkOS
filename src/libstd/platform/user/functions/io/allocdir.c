#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

allocdir_t* allocdir(fd_t fd)
{
    while (true)
    {
        uint64_t amount = readdir(fd, NULL, 0);
        if (amount == ERR)
        {
            return NULL;
        }

        allocdir_t* dirs = malloc(sizeof(allocdir_t) + sizeof(stat_t) * amount);
        if (dirs == NULL)
        {
            return NULL;
        }

        dirs->amount = amount;
        if (readdir(fd, dirs->infos, amount) == ERR)
        {
            free(dirs);
            return NULL;
        }

        uint64_t newAmount = readdir(fd, NULL, 0);
        if (newAmount == ERR)
        {
            free(dirs);
            return NULL;
        }

        if (newAmount == amount)
        {
            return dirs;
        }
        else
        {
            free(dirs);
        }
    }
}

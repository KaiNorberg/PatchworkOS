#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "tty/tty.h"
#include "heap/heap.h"
#include "array/array.h"
#include "sched/sched.h"
#include "vfs/utils/utils.h"

static Inode* root;

void vfs_init()
{
    tty_start_message("VFS initializing");

    root = 0;

    tty_end_message(TTY_MESSAGE_OK);
}

uint64_t vfs_open(const char* path, uint64_t flags)
{
    return ERR;
}

uint64_t vfs_close(uint64_t fd)
{
    return ERR;
}
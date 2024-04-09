#include "sysfs.h"

#include <string.h>

#include "tty/tty.h"
#include "vfs/utils/utils.h"

/*static Filesystem sysfs;
static SysNode* root;*/

/*static SysNode* sysfs_traverse(const char* path)
{

}*/

void sysfs_init()
{
    tty_start_message("Sysfs initializing");

    /*memset(&sysfs, 0, sizeof(Filesystem));
    sysfs.name = "sysfs";

    if (vfs_mount('A', &sysfs, NULL) == ERR)
    {
        tty_print("Failed to mount sysfs");
        tty_end_message(TTY_MESSAGE_ER);
    }*/

    tty_end_message(TTY_MESSAGE_OK);
}
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/proc.h>

#include "user/common/exit_stack.h"
#include "user/common/file.h"

void exit(int status)
{
    proc_exit(F("%d", status));
}

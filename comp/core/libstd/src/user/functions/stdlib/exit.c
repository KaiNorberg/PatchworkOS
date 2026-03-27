#include <libstd/fs.h>
#include <libstd/io.h>
#include <libstd/proc.h>
#include <stdlib.h>

#include "user/common/exit_stack.h"
#include "user/common/file.h"

void exit(int status)
{
    proc_exit(IOFMT("%d", status));
}

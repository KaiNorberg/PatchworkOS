#include <stdlib.h>

#include "user/common/exit_stack.h"
#include "user/common/file.h"
#include "user/common/syscalls.h"

void exit(int status)
{
    _exit(F("%d", status));
}

#include <stdlib.h>

#include "platform/user/common/exit_stack.h"
#include "platform/user/common/file.h"
#include "platform/user/common/syscalls.h"

void exit(int status)
{
    _ExitStackDispatch();
    _FilesClose();
    _SyscallProcessExit(status);
}

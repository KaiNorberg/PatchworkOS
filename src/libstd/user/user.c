#include "user.h"

#include "common/clock.h"
#include "common/exit_stack.h"
#include "common/std_streams.h"
#include "common/threading.h"
#include "user/common/file.h"
#include "user/common/note.h"

#include <sys/fs.h>
#include <sys/proc.h>

static void _populate_std_descriptors(void)
{
    for (uint64_t i = 0; i <= STDERR_FILENO; i++)
    {
        if (write(i, NULL, 0) == _FAIL && errno == EBADF)
        {
            fd_t nullFd = open("/dev/const/null");
            if (nullFd != i)
            {
                dup2(nullFd, i);
                close(nullFd);
            }
        }
    }
}

void _user_init(void)
{
    _clock_init();
    _threading_init();
    _populate_std_descriptors();
    _exit_stack_init();
    _files_init();
    _std_streams_init();
    _note_init();
}

#include "user.h"

#include "common/clock.h"
#include "common/exit_stack.h"
#include "common/std_streams.h"
#include "common/threading.h"
#include "user/common/file.h"
#include "user/common/io.h"
#include "user/common/note.h"

#include <sys/fs.h>
#include <sys/proc.h>
#include <sys/status.h>

static void _populate_std_descriptors(void)
{
    for (fd_t i = 0; i <= STDERR_FILENO; i++)
    {
        status_t status = ioseek(i, IOSEEK_CUR, 0, NULL);
        if (!IS_CODE(status, BADFD))
        {
            continue;
        }

        fd_t nullFd;
        status = open(&nullFd, "/dev/const/null:rw");
        if (IS_ERR(status))
        {
            continue;
        }

        fd_t targetFd = i;
        if (nullFd != targetFd)
        {
            dup(nullFd, &targetFd);
            close(nullFd);
        }
    }
}

void _user_init(void)
{
    _clock_init();
    _threading_init();
    _io_init();
    _populate_std_descriptors();
    _exit_stack_init();
    _files_init();
    _std_streams_init();
    _note_init();
}

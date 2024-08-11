#include "shell.h"

#include <stdlib.h>
#include <string.h>
#include <sys/dwm.h>
#include <sys/win.h>

static win_t** windows;
static uint32_t windowAmount;

void shell_init(void)
{
    windows = NULL;
    windowAmount = 0;
}

void shell_loop(void)
{
    while (windowAmount != 0)
    {
        pollfd_t fds[64];
        for (uint64_t i = 0; i < windowAmount; i++)
        {
            fds[i] = (pollfd_t){.fd = win_fd(windows[i]), .requested = POLL_READ};
        }
        poll(fds, windowAmount, NEVER);

        for (int64_t i = 0; i < windowAmount; i++)
        {
            if (!(fds[i].occurred & POLL_READ))
            {
                continue;
            }

            msg_t msg = {0};
            while (win_receive(windows[i], &msg, 0))
            {
                win_dispatch(windows[i], &msg);

                if (msg.type == LMSG_QUIT)
                {
                    win_free(windows[i]);

                    if (i != windowAmount - 1)
                    {
                        memmove(&windows[i], &windows[i + 1], sizeof(win_t*) * (windowAmount - i - 1));
                    }
                    windowAmount--;
                    i--;
                    break;
                }
            }
        }
    }
}

void shell_push(win_t* window)
{
    if (windows == NULL)
    {
        windows = malloc(sizeof(win_t*));
    }
    else
    {
        windows = realloc(windows, sizeof(win_t*) * (windowAmount + 1));
    }
    windows[windowAmount++] = window;
}

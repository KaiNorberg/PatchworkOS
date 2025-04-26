#include "dwm.h"

#include <sys/io.h>

static fd_t handle;
static char id[MAX_NAME];

static fd_t fb;

void dwm_init(void)
{
    fd_t handle = open("sys:/net/local/new");
    read(handle, id, MAX_NAME);
}

void dwm_deinit(void)
{
    close(handle);
}

void dwm_loop(void)
{

}

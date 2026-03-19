#include <sys/comp.h>

status_t comp_launch(fd_t cwd, fd_t root, const char* name)
{
    UNUSED(cwd);
    UNUSED(root);

    if (name == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    return OK;
}
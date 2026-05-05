#include "common/path_mode.h"

path_mode_t path_posix_to_mode(mode_t mode)
{
    path_mode_t res = PATH_MODE_NONE;

    if (mode & (S_IRUSR | S_IRGRP | S_IROTH))
    {
        res |= PATH_MODE_READ;
    }
    if (mode & (S_IWUSR | S_IWGRP | S_IWOTH))
    {
        res |= PATH_MODE_WRITE;
    }
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
    {
        res |= PATH_MODE_EXECUTE;
    }

    if (S_ISDIR(mode))
    {
        res |= PATH_MODE_DIRECTORY;
    }
    else if (S_ISLNK(mode))
    {
        res |= PATH_MODE_SYMLINK;
    }

    return res;
}
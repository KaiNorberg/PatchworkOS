#include "common/path_mode.h"

mode_t path_mode_to_posix(path_mode_t mode)
{
    mode_t res = 0;

    if (mode & PATH_MODE_READ)
    {
        res |= S_IRUSR | S_IRGRP | S_IROTH;
    }
    if (mode & PATH_MODE_WRITE)
    {
        res |= S_IWUSR | S_IWGRP | S_IWOTH;
    }
    if (mode & PATH_MODE_EXECUTE)
    {
        res |= S_IXUSR | S_IXGRP | S_IXOTH;
    }

    if (mode & PATH_MODE_DIRECTORY)
    {
        res |= S_IFDIR;
    }
    else if (mode & PATH_MODE_SYMLINK)
    {
        res |= S_IFLNK;
    }
    else
    {
        res |= S_IFREG;
    }

    return res;
}
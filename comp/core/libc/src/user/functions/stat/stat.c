#include <libc/io.h>
#include <sys/stat.h>
#include <sys/types.h>

int stat(const char* path, struct stat* buf)
{
    file_info_t info;
    if (ioqueryp(FDCWD, FDROOT, path, &info) < 0)
    {
        return -1;
    }

    mode_t mode = 0;
    status_t status = path_string_to_posix(info.mode, strlen(info.mode), &mode);
    if (IS_ERR(status))
    {
        return -1;
    }

    buf->st_dev = info.volume;
    buf->st_ino = info.number;
    buf->st_mode = mode;
    buf->st_nlink = info.nlink;
    buf->st_uid = -1;
    buf->st_gid = -1;
    buf->st_rdev = -1;
    buf->st_size = info.size;
    buf->st_blksize = info.blockSize;
    buf->st_blocks = info.blocks;
    buf->st_atime = info.atime;
    buf->st_mtime = info.mtime;
    buf->st_ctime = info.ctime;

    return 0;
}
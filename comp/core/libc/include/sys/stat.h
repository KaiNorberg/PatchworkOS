#ifndef _SYS_STAT_H
#define _SYS_STAT_H 1

#ifdef __cplusplus
extern "C"
{
#endif

// https://pubs.opengroup.org/onlinepubs/007908799/xsh/sysstat.h.html

#include <assert.h>
#include <sys/types.h>

struct stat
{
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

#define S_IFMT 0170000
#define S_IFBLK 0060000
#define S_IFCHR 0020000
#define S_IFIFO 0010000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFLNK 0120000

#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)

#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)

#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)

#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)

#define S_TYPEISMQ(buf) (0)
#define S_TYPEISSEM(buf) (0)
#define S_TYPEISSHM(buf) (0)

static inline int chmod(const char* path, mode_t mode)
{
    assert(0 && "chmod is not supported");
    return 0;
}

static inline int fchmod(int fd, mode_t mode)
{
    assert(0 && "fchmod is not supported");
    return 0;
}

int fstat(int fd, struct stat* buf);

int lstat(const char* path, struct stat* buf);

int mkdir(const char* path, mode_t mode);

static inline int mkfifo(const char* path, mode_t mode)
{
    assert(0 && "mkfifo is not supported");
    return 0;
}

static inline int mknod(const char* path, mode_t mode, dev_t dev)
{
    assert(0 && "mknod is not supported");
    return 0;
}

int stat(const char* path, struct stat* buf);

static inline mode_t umask(mode_t mask)
{
    assert(0 && "umask is not supported");
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
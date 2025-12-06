#include <sys/io.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

uint64_t rmdir(const char* path)
{
    return remove(F("%s:directory", path));
}

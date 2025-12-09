#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

uint64_t rmdir(const char* path)
{
    return remove(F("%s:directory", path));
}

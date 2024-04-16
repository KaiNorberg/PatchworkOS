#include "volume.h"

#include <string.h>

void volume_init(Volume* volume, Filesystem* fs)
{
    memset(volume, 0, sizeof(Volume));
    volume->fs = fs;
    atomic_init(&volume->ref, 1);
}
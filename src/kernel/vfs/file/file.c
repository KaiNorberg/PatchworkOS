#include "file.h"

#include <string.h>

void file_init(File* file, Volume* volume)
{
    memset(file, 0, sizeof(File));
    file->volume = volume;
    file->position = 0;
    atomic_init(&file->ref, 1);
}
#include <sys/bitmap.h>

void bitmap_init(bitmap_t* map, void* buffer, uint64_t length)
{
    map->firstZeroIdx = 0;
    map->length = length;
    map->buffer = (uint64_t*)buffer;
}
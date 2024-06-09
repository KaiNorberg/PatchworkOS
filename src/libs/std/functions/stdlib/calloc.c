#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void* calloc(size_t num, size_t size)
{
    void* data = malloc(num * size);
    memset(data, 0, num * size);
    return data;
}
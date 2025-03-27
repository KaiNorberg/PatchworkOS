#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/io.h>

#define FLAG_ALL (1 << 0)
#define FLAG_RECURSIVE (1 << 1)

static uint64_t flags;

typedef struct
{

} flag_map_entry_t;

static flag_map_entry_t flagMap[] = 
{

};

int main(int argc, char** argv)
{
    for (int i = 0; i < argc; i++)
    {
        uint64_t strLen = strnlen(argv[i], MAX_PATH);
        if (strLen >= MAX_PATH - 1)
        {
            printf("Did you try to cause an overflow on purpose?");
        }

        if (argv[i][0] == '-')
        {
            for (uint64_t j = 0; j < strLen - 1; j++)
            {

            }
        }
    }
    return 0;
}
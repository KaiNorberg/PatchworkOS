#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

#define FLAG_ALL (1 << 0)
#define FLAG_RECURSIVE (1 << 1)

typedef uint64_t flags_t;

typedef struct
{
    flags_t flag;
    char chr;
    const char* string;
} flag_map_entry_t;

static flag_map_entry_t flagMap[] = {{FLAG_ALL, 'a', "all"}, {FLAG_RECURSIVE, 'R', "recursive"}};

typedef struct
{
    flags_t flags;
    const char** paths;
} args_t;

uint64_t args_parse(args_t* args, int argc, char** argv)
{
    args->flags = 0;
    args->paths = NULL;

    for (int i = 0; i < argc; i++)
    {
        if (strnlen(argv[i], MAX_PATH) >= MAX_PATH - 1)
        {
            printf("Did you try to cause an overflow on purpose?");
            continue;
        }

        if (argv[i][0] != '-')
        {

            continue;
        }

        if (argv[i][1] == '-')
        {
            for (size_t j = 0; j < sizeof(flagMap) / sizeof(flagMap[0]); j++)
            {
                if (strcmp(flagMap[j].string, argv[i] + 2) == 0)
                {
                    args->flags |= flagMap[j].flag;
                }
            }
        }
        else
        {
            for (size_t j = 1; argv[i][j]; j++)
            {
                for (size_t k = 0; k < sizeof(flagMap) / sizeof(flagMap[0]); k++)
                {
                    if (argv[i][j] == flagMap[k].chr)
                    {
                        args->flags |= flagMap[k].flag;
                    }
                }
            }
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    args_t args;
    if (args_parse(&args, argc, argv) == -1ULL)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
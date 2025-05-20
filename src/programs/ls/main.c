#define __STDC_WANT_LIB_EXT1__ 1
#include <errno.h>
#include <stdbool.h>
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
    uint64_t pathAmount;
} args_t;

uint64_t args_init(args_t* args, int argc, char** argv)
{
    args->flags = 0;
    args->paths = malloc(sizeof(const char*) * argc);
    args->pathAmount = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strnlen_s(argv[i], MAX_PATH) >= MAX_PATH - 1)
        {
            fprintf(stderr, "ls: Did you try to cause an overflow on purpose?\n");
            continue;
        }

        if (argv[i][0] != '-')
        {
            args->paths[args->pathAmount++] = argv[i];
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

void args_deinit(args_t* args)
{
    free(args->paths);
}

uint64_t print_directory(const char* path, flags_t flags, bool forceLabel)
{
    if (flags & FLAG_RECURSIVE || forceLabel)
    {
        printf("[%s]\n", path);
    }

    fd_t fd = openf("%s?dir", path);
    if (fd == ERR)
    {
        fprintf(stderr, "ls: cant open directory %s (%s)\n", path, strerror(errno));
        return ERR;
    }
    allocdir_t* dirs = allocdir(fd);
    close(fd);
    if (dirs == NULL)
    {
        fprintf(stderr, "ls: cant read directory %s (%s)\n", path, strerror(errno));
        return ERR;
    }

    for (uint64_t i = 0; i < dirs->amount; i++)
    {
        if (dirs->infos[i].type == STAT_FILE)
        {
            printf("%s ", dirs->infos[i].name);
        }
        else
        {
            printf("%s/ ", dirs->infos[i].name);
        }
    }
    printf("\n");

    if (flags & FLAG_RECURSIVE)
    {
        for (uint64_t i = 0; i < dirs->amount; i++)
        {
            if (dirs->infos[i].type == STAT_DIR)
            {
                char buffer[MAX_PATH];
                snprintf(buffer, MAX_PATH, "%s/%s", path, dirs->infos[i].name);

                print_directory(buffer, flags, forceLabel);
            }
        }
    }

    return 0;
}

int main(int argc, char** argv)
{
    args_t args;
    if (args_init(&args, argc, argv) == -1ULL)
    {
        return EXIT_FAILURE;
    }

    if (args.pathAmount == 0)
    {
        print_directory(".", args.flags, false);
    }
    else
    {
        for (uint64_t i = 0; i < args.pathAmount; i++)
        {
            print_directory(args.paths[i], args.flags, args.pathAmount > 1);
        }
    }

    args_deinit(&args);
    return EXIT_SUCCESS;
}

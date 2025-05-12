#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        while (1)
        {
            char chr = getchar();
            if (chr == EOF)
            {
                break;
            }

            putchar(chr);
        }
    }

    for (int i = 1; i < argc; i++)
    {
        FILE* file = fopen(argv[i], "r");
        if (file == NULL)
        {
            fprintf(stderr, "cat: can't open %s (%s)\n", argv[i], strerror(errno));
            continue;
        }

        while (1)
        {
            char chr = fgetc(file);
            if (chr == EOF)
            {
                break;
            }

            putchar(chr);
        }

        fclose(file);
    }

    return EXIT_SUCCESS;
}

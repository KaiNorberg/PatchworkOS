#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

// TODO: Fix this very basic and wrong echo implementation
int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        printf(argv[i]);
        if (i != argc - 1)
        {
            printf(" ");
        }
    }
    return EXIT_SUCCESS;
}
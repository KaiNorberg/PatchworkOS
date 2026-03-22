#include <errno.h>
#include <stdio.h>
#include <string.h>

void perror(const char* s)
{
    if ((s != NULL) && (s[0] != '\n'))
    {
        fprintf(stderr, "%s", s);
    }

    fprintf(stderr, " (%s)\n", strerror(errno));

    return;
}
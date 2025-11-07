#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: grep <pattern>\n");
        return 1;
    }

    const char* pattern = argv[1];
    char line[1024];

    while (fgets(line, sizeof(line), stdin) != NULL)
    {
        if (strstr(line, pattern) == NULL)
        {
            continue;
        }

        bool inMatch = false;
        for (char* p = line; *p != '\0'; p++)
        {
            if (strncmp(p, pattern, strlen(pattern)) == 0)
            {
                if (!inMatch)
                {
                    printf("\033[31m");
                    inMatch = true;
                }
                printf("%.*s", (int)strlen(pattern), pattern);
                p += strlen(pattern) - 1;
            }
            else
            {
                if (inMatch)
                {
                    printf("\033[0m");
                    inMatch = false;
                }
                putchar(*p);
            }
        }
    }

    return 0;
}
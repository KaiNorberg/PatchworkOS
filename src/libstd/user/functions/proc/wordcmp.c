#include <sys/proc.h>

#include <string.h>

int64_t wordcmp(const char* string, const char* word)
{
    size_t len = strlen(word);
    return strncmp(string, word, len) == 0 && (string[len] == '\0' || string[len] == ' ') ? 0 : -1;
}
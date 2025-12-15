#include <sys/proc.h>

#include <string.h>

int64_t notecmp(const char* note, const char* word)
{
    size_t len = strlen(word);
    return strncmp(note, word, len) == 0 && (note[len] == '\0' || note[len] == ' ') ? 0 : -1;
}
#include "common/argsplit.h"

const char** argsplit_buf(void* buf, uint64_t size, const char* str, uint64_t maxLen, uint64_t* count)
{
    uint64_t skipped = 0;
    while (isspace(*str) && (maxLen == 0 || skipped < maxLen))
    {
        str++;
        skipped++;
    }
    maxLen = (maxLen == 0) ? 0 : (maxLen > skipped ? maxLen - skipped : 0);

    uint64_t argc;
    uint64_t totalChars;
    if (_ArgsplitCountCharsAndArgs(str, &argc, &totalChars, maxLen) == UINT64_MAX)
    {
        return NULL;
    }

    uint64_t argvSize = sizeof(char*) * (argc + 1);
    uint64_t stringsSize = totalChars + argc;

    const char** argv = buf;
    if (size < argvSize + stringsSize)
    {
        return NULL;
    }
    if (count != NULL)
    {
        *count = argc;
    }
    if (argc == 0)
    {
        argv[0] = NULL;
        return argv;
    }

    return _ArgsplitBackend(buf, str, argc, maxLen);
}

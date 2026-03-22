#include <stdio.h>
#include <stdlib.h>
#include <sys/proc.h>

#include "user/common/file.h"

int fclose(struct FILE* stream)
{
    int status = _file_deinit(stream);
    if (stream != stdin && stream != stdout && stream != stderr)
    {
        free(stream);
    }
    return status;
}

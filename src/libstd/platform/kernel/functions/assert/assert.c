#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "log/log.h"

void _Assert99(const char* const message1, const char* const function, const char* const message2)
{
    log_panic(NULL, "%s %s %s", message1, function, message2);
}

void _Assert89(const char* const message)
{
    log_panic(NULL, message);
}

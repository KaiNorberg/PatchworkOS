#include "testing.h"
#ifdef TESTING

#include "log/log.h"

#include <assert.h>

void path_all_tests(void);
void testing_run_tests(void)
{
    log_print(LOG_INFO, "testing: running tests\n");
    const uint64_t count = _testsEnd - _testsStart;
    for (uint64_t i = 0; i < count; i++)
    {
        log_print(LOG_INFO, "testing: running %s\n", _testsStart[i].name);
        assert(_testsStart[i].func() != ERR);
    }
    log_print(LOG_INFO, "testing: finished tests\n");
}

#endif

#include "testing.h"
#ifdef TESTING

#include "log.h"

#include <stdio.h>

void path_all_tests(void);
void testing_run_tests(void)
{
    printf("testing: running tests");
    const uint64_t count = _testsEnd - _testsStart;
    for (uint64_t i = 0; i < count; i++)
    {
        printf("testing: running %s", _testsStart[i].name);
        ASSERT_PANIC(_testsStart[i].func() != ERR);
    }
    printf("testing: finished tests");
}

#endif

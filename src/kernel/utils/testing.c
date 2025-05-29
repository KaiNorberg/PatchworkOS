#include "testing.h"
#ifdef TESTING

#include "log.h"

#include <assert.h>
#include <stdio.h>

void path_all_tests(void);
void testing_run_tests(void)
{
    printf("testing: running tests\n");
    const uint64_t count = _testsEnd - _testsStart;
    for (uint64_t i = 0; i < count; i++)
    {
        printf("testing: running %s\n", _testsStart[i].name);
        assert(_testsStart[i].func() != ERR);
    }
    printf("testing: finished tests\n");
}

#endif

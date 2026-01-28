#include <string.h>

char* strncpy(char* _RESTRICT s1, const char* _RESTRICT s2, size_t n)
{
    char* rc = s1;

    while (n && (*s1++ = *s2++))
    {
        /* Cannot do "n--" in the conditional as size_t is unsigned and we have
           to check it again for >0 in the next loop below, so we must not risk
           underflow.
        */
        --n;
    }

    /* Checking against 1 as we missed the last --n in the loop above. */
    while (n-- > 1)
    {
        *s1++ = '\0';
    }

    return rc;
}

#ifdef _KERNEL_
#ifdef _TESTING_

#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/utils/test.h>

static uint64_t _test_strncpy_iter(void)
{
    char s[] = "xxxxxxx";
    TEST_ASSERT(strncpy(s, "", 1) == s);
    TEST_ASSERT(s[0] == '\0');
    TEST_ASSERT(s[1] == 'x');
    TEST_ASSERT(strncpy(s, "abcde", 6) == s);
    TEST_ASSERT(s[0] == 'a');
    TEST_ASSERT(s[4] == 'e');
    TEST_ASSERT(s[5] == '\0');
    TEST_ASSERT(s[6] == 'x');
    TEST_ASSERT(strncpy(s, "abcde", 7) == s);
    TEST_ASSERT(s[6] == '\0');
    TEST_ASSERT(strncpy(s, "xxxx", 3) == s);
    TEST_ASSERT(s[0] == 'x');
    TEST_ASSERT(s[2] == 'x');
    TEST_ASSERT(s[3] == 'd');

    char s2[1024];
    memset(s2, 'x', sizeof(s2));
    char src[512];
    memset(src, 'a', sizeof(src));
    src[511] = '\0';

    TEST_ASSERT(strncpy(s2, src, 1024) == s2);
    TEST_ASSERT(s2[0] == 'a');
    TEST_ASSERT(s2[510] == 'a');
    TEST_ASSERT(s2[511] == '\0');
    TEST_ASSERT(s2[512] == '\0');
    TEST_ASSERT(s2[1023] == '\0');

    return 0;
}

TEST_DEFINE(strncpy)
{
    for (int k = 0; k < 1; ++k)
    {
        TEST_ASSERT(_test_strncpy_iter() != PFAIL);
    }

    return 0;
}

#endif
#endif
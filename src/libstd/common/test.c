#include <kernel/sched/clock.h>
#include <time.h>

#ifdef _KERNEL_
#ifdef _TESTING_


#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/utils/test.h>

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INT_MIN_DEZ_STR "2147483648"
#define INT_MAX_DEZ_STR "2147483647"
#define UINT_MAX_DEZ_STR "4294967295"
#define INT_HEXDIG "fffffff"
#define INT_OCTDIG "37777777777"

static int _vsscanf_wrapper(const char* str, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsscanf(str, format, ap);
    va_end(ap);
    return ret;
}

#define SCANF_TEST(rc, input, fmt, ...)                                                                    \
    do                                                                                                     \
    {                                                                                                      \
        int ret = _vsscanf_wrapper(input, fmt, ##__VA_ARGS__);                                             \
        if (ret != rc)                                                                                     \
        {                                                                                                  \
            LOG_ERR("SCANF_TEST failed at line %d: expected %d, got %d", __LINE__, rc, ret);             \
        }                                                                                                  \
    } while (0)

static inline uint64_t _test_vsscanf_iter(void)
{
    char buffer[100];
    int i;
    unsigned int u;
    int* p;
    int n;

    /* basic: reading of three-char string */
    SCANF_TEST(1, "foo", "%3c", buffer);
    TEST_ASSERT(memcmp(buffer, "foo", 3) == 0);

    /* %% for single % */
    SCANF_TEST(1, "%x", "%%%c%n", buffer, &n);
    TEST_ASSERT(n == 2);
    TEST_ASSERT(buffer[0] == 'x');
    /* * to skip assignment */
    SCANF_TEST(0, "abcdefg", "%*[cba]%n", &n);
    TEST_ASSERT(n == 3);
    SCANF_TEST(0, "foo", "%*s%n", &n);
    TEST_ASSERT(n == 3);
    SCANF_TEST(0, "abc", "%*c%n", &n);
    TEST_ASSERT(n == 1);
    SCANF_TEST(1, "3xfoo", "%*dx%3c", buffer);
    TEST_ASSERT(memcmp(buffer, "foo", 3) == 0);

    /* domain testing on 'int' type */
    SCANF_TEST(1, "-" INT_MIN_DEZ_STR, "%d", &i);
    TEST_ASSERT(i == INT_MIN);
    SCANF_TEST(1, INT_MAX_DEZ_STR, "%d", &i);
    TEST_ASSERT(i == INT_MAX);
    SCANF_TEST(1, "-1", "%d", &i);
    TEST_ASSERT(i == -1);
    SCANF_TEST(1, "0", "%d", &i);
    TEST_ASSERT(i == 0);
    SCANF_TEST(1, "1", "%d", &i);
    TEST_ASSERT(i == 1);
    SCANF_TEST(1, "-" INT_MIN_DEZ_STR, "%i", &i);
    TEST_ASSERT(i == INT_MIN);
    SCANF_TEST(1, INT_MAX_DEZ_STR, "%i", &i);
    TEST_ASSERT(i == INT_MAX);
    SCANF_TEST(1, "-1", "%i", &i);
    TEST_ASSERT(i == -1);
    SCANF_TEST(1, "0", "%i", &i);
    TEST_ASSERT(i == 0);
    SCANF_TEST(1, "1", "%i", &i);
    TEST_ASSERT(i == 1);
    SCANF_TEST(1, "0x7" INT_HEXDIG, "%i", &i);
    TEST_ASSERT(i == INT_MAX);
    SCANF_TEST(1, "0x0", "%i", &i);
    TEST_ASSERT(i == 0);
    SCANF_TEST(1, "00", "%i%n", &i, &n);
    TEST_ASSERT(i == 0);
    TEST_ASSERT(n == 2);

    /* domain testing on 'unsigned int' type */
    SCANF_TEST(1, UINT_MAX_DEZ_STR, "%u", &u);
    TEST_ASSERT(u == UINT_MAX);
    SCANF_TEST(1, "0", "%u", &u);
    TEST_ASSERT(u == 0);
    SCANF_TEST(1, "f" INT_HEXDIG, "%x", &u);
    TEST_ASSERT(u == UINT_MAX);
    SCANF_TEST(1, "7" INT_HEXDIG, "%x", &u);
    TEST_ASSERT(u == INT_MAX);
    SCANF_TEST(1, "0", "%o", &u);
    TEST_ASSERT(u == 0);
    SCANF_TEST(1, INT_OCTDIG, "%o", &u);
    TEST_ASSERT(u == UINT_MAX);
    /* testing %c */
    memset(buffer, '\0', 100);
    SCANF_TEST(1, "x", "%c", buffer);
    TEST_ASSERT(memcmp(buffer, "x\0", 2) == 0);
    /* testing %s */
    memset(buffer, '\0', 100);
    SCANF_TEST(1, "foo bar", "%s%n", buffer, &n);
    TEST_ASSERT(memcmp(buffer, "foo\0", 4) == 0);
    TEST_ASSERT(n == 3);
    SCANF_TEST(2, "foo bar  baz", "%s %s %n", buffer, buffer + 4, &n);
    TEST_ASSERT(n == 9);
    TEST_ASSERT(memcmp(buffer, "foo\0bar\0", 8) == 0);
    /* testing %[ */
    SCANF_TEST(1, "abcdefg", "%[cba]", buffer);
    TEST_ASSERT(memcmp(buffer, "abc\0", 4) == 0);
    SCANF_TEST(-1, "", "%[cba]", buffer);
    SCANF_TEST(1, "3", "%u%[cba]", &u, buffer);
    /* testing %p */
    p = NULL;
    sprintf(buffer, "%p", (void*)p);
    p = &i;
    SCANF_TEST(1, buffer, "%p", (void**)&p);
    TEST_ASSERT(p == NULL);
    p = &i;
    sprintf(buffer, "%p", (void*)p);
    p = NULL;
    SCANF_TEST(1, buffer, "%p", (void**)&p);
    TEST_ASSERT(p == &i);
    /* errors */
    SCANF_TEST(EOF, "", "%d", &i);
    SCANF_TEST(1, "foo", "%5c", buffer);
    TEST_ASSERT(memcmp(buffer, "foo", 3) == 0);

    return 0;
}

TEST_DEFINE(vsscanf)
{
    char buffer[100];
    int i;
    unsigned int u;
    int n;

    clock_t start = clock_uptime();
    for (int k = 0; k < 100000; ++k)
    {
        TEST_ASSERT(_test_vsscanf_iter() != ERR);
    }
    clock_t end = clock_uptime();

    return 0;
}

#endif
#endif
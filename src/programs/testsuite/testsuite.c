// Ignore this file for now

#define TEST(input, ...) \
    do \
    { \
        const char* expected[] = {__VA_ARGS__, NULL}; \
        if (!test_argsplit(input, expected)) \
        { \
            printf("###### ^FAIL^", input); \
        } \
        else \
        { \
            printf("######", input); \
        } \
    } while (0)

bool test_argsplit(const char* input, const char** expected)
{
    static uint64_t counter = 0;

    uint64_t argc;
    const char** argv = argsplit(input, &argc);
    if (!argv)
    {
        counter++;
        return false;
    }

    printf("test (%d): |%s|, %d", counter++, input, argc);

    for (uint64_t i = 0; i < argc; i++)
    {
        if (expected[i] == NULL || argv[i] == NULL)
        {
            free(argv);
            printf("length error");
            return false;
        }

        printf("expected: |%s|, actual: |%s|", expected[i], argv[i]);
    }

    for (uint64_t i = 0; i < argc; i++)
    {
        if (strcmp(expected[i], argv[i]) != 0)
        {
            free(argv);
            return false;
        }
    }

    free(argv);
    return true;
}

TEST("a b c", "a", "b", "c");
TEST("  a   b  ", "a", "b");
TEST("\"a b\" c", "a b", "c");
TEST("a \"b c\" \"d e\"", "a", "b c", "d e");
TEST("a\\ b", "a b");
TEST("\"a\\ b\"", "a b");
TEST("\"a\\\\b\"", "a\\b");
TEST("a\"b c\"d", "a", "b c", "d");
TEST("ls -l \"My Documents\"", "ls", "-l", "My Documents");
TEST("echo \"Hello\\nWorld\"", "echo", "HellonWorld");

while (true)
    ;
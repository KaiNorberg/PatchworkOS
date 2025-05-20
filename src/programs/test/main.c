#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void first();
static void second();

/* Use a file scoped static variable for the exception stack so we can access
 * it anywhere within this translation unit. */
static jmp_buf exception_env;
static int exception_type;

int main(void)
{
    printf("expected output:\ncalling first\nentering first\ncalling second\nentering second\nsecond failed, exception type: 3; remapping to type 1\nfirst failed, exception type: 1\n\nactual output:\n");

    char* volatile mem_buffer = NULL;

    if (setjmp(exception_env))
    {
        // if we get here there was an exception
        printf("first failed, exception type: %d\n", exception_type);
    }
    else
    {
        // Run code that may signal failure via longjmp.
        puts("calling first");
        first();

        mem_buffer = malloc(300);                              // allocate a resource
        printf("%s\n", strcpy(mem_buffer, "first succeeded")); // not reached
    }

    free(mem_buffer); // NULL can be passed to free, no operation is performed

    return 0;
}

static void first()
{
    jmp_buf my_env;

    puts("entering first"); // reached

    memcpy(my_env, exception_env,
        sizeof my_env); // store value of exception_env in my_env since exception_env will be reused

    switch (setjmp(exception_env))
    {
    case 3: // if we get here there was an exception.
        puts("second failed, exception type: 3; remapping to type 1");
        exception_type = 1;

    default:                                                 // fall through
        memcpy(exception_env, my_env, sizeof exception_env); // restore exception stack
        longjmp(exception_env, exception_type);              // continue handling the exception

    case 0:                     // normal, desired operation
        puts("calling second"); // reached
        second();
        puts("second succeeded"); // not reached
    }

    memcpy(exception_env, my_env, sizeof exception_env); // restore exception stack

    puts("leaving first"); // never reached
}

static void second()
{
    puts("entering second"); // reached

    exception_type = 3;
    longjmp(exception_env, exception_type); // declare that the program has failed

    puts("leaving second"); // not reached
}
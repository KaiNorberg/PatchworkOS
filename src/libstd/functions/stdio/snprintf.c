#include <stdio.h>

int snprintf(char* _RESTRICT s, size_t n, const char* _RESTRICT format, ...)
{
    int rc;
    va_list ap;
    va_start(ap, format);
    rc = vsnprintf(s, n, format, ap);
    va_end(ap);
    return rc;
}

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
#include <time.h>

#define sym2v(x) #x
#define sym2s(x) sym2v(x)

#define INT_MIN_DEZ_STR "2147483648"
#define INT_MAX_DEZ_STR "2147483647"
#define UINT_MAX_DEZ_STR "4294967295"
#define INT_HEXDIG "fffffff"
#define INT_hexdig "fffffff"
#define INT_OCTDIG "37777777777"
#define UINT_DIG 10
#define INT_DIG 10
#define INT_DIG_LESS1 "9"
#define INT_DIG_PLUS1 "11"
#define INT_DIG_PLUS2 "12"
#define ULONG_DIG 20
#define LONG_DIG 19
#define LONG_MAX_DEZ_STR "9223372036854775807"
#define LONG_MIN_DEZ_STR "9223372036854775808"
#define ULONG_MAX_DEZ_STR "18446744073709551615"
#define ULLONG_DIG 20
#define LLONG_DIG 19
#define LLONG_MAX_DEZ_STR "9223372036854775807"
#define LLONG_MIN_DEZ_STR "9223372036854775808"
#define ULLONG_MAX_DEZ_STR "18446744073709551615"

#define _PRINT_TEST(rc, expected, ...) \
    ({ \
        char buffer[256]; \
        int ret = snprintf(buffer, sizeof(buffer), __VA_ARGS__); \
        if (ret != rc || strcmp(buffer, expected) != 0) \
        { \
            LOG_ERR("_PRINT_TEST failed at line %d: expected (%d, \"%s\"), got (%d, \"%s\")\n", __LINE__, rc, \
                expected, ret, buffer); \
            return ERR; \
        } \
    })

static inline uint64_t _test_print_iter(void)
{
#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif
#if CHAR_MIN == -128
    assert(CHAR_MIN == -128);
    _PRINT_TEST(4, "-128", "%hhd", CHAR_MIN);
    assert(CHAR_MAX == 127);
    _PRINT_TEST(3, "127", "%hhd", CHAR_MAX);
#else
    assert(CHAR_MIN == 0);
    _PRINT_TEST(1, "0", "%hhu", CHAR_MIN);
    assert(CHAR_MAX == 255);
    _PRINT_TEST(3, "255", "%hhu", CHAR_MAX);
#endif
    _PRINT_TEST(1, "0", "%hhd", 0);
    assert(SHRT_MIN == -32768);
    _PRINT_TEST(6, "-32768", "%hd", SHRT_MIN);
    assert(SHRT_MAX == 32767);
    _PRINT_TEST(5, "32767", "%hd", SHRT_MAX);
    _PRINT_TEST(1, "0", "%hd", 0);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%d", INT_MAX);
    _PRINT_TEST(1, "0", "%d", 0);
    _PRINT_TEST(LONG_DIG + 1, "-" LONG_MIN_DEZ_STR, "%ld", LONG_MIN);
    _PRINT_TEST(LONG_DIG, LONG_MAX_DEZ_STR, "%ld", LONG_MAX);
    _PRINT_TEST(1, "0", "%ld", 0l);
    _PRINT_TEST(LLONG_DIG + 1, "-" LLONG_MIN_DEZ_STR, "%lld", LLONG_MIN);
    _PRINT_TEST(LLONG_DIG, LLONG_MAX_DEZ_STR, "%lld", LLONG_MAX);
    _PRINT_TEST(1, "0", "%lld", 0ll);
    _PRINT_TEST(3, "255", "%hhu", UCHAR_MAX);
    _PRINT_TEST(3, "255", "%hhu", (unsigned char)-1);
    _PRINT_TEST(5, "65535", "%hu", USHRT_MAX);
    _PRINT_TEST(5, "65535", "%hu", (unsigned short)-1);
    _PRINT_TEST(UINT_DIG, UINT_MAX_DEZ_STR, "%u", UINT_MAX);
    _PRINT_TEST(UINT_DIG, UINT_MAX_DEZ_STR, "%u", -1u);
    _PRINT_TEST(ULONG_DIG, ULONG_MAX_DEZ_STR, "%lu", ULONG_MAX);
    _PRINT_TEST(ULONG_DIG, ULONG_MAX_DEZ_STR, "%lu", -1ul);
    _PRINT_TEST(ULLONG_DIG, ULLONG_MAX_DEZ_STR, "%llu", ULLONG_MAX);
    _PRINT_TEST(ULLONG_DIG, ULLONG_MAX_DEZ_STR, "%llu", -1ull);
    _PRINT_TEST((int)strlen(INT_HEXDIG) + 1, "f" INT_hexdig, "%x", UINT_MAX);
    _PRINT_TEST((int)strlen(INT_HEXDIG) + 3, "0xf" INT_hexdig, "%#x", -1u);
    _PRINT_TEST((int)strlen(INT_OCTDIG), INT_OCTDIG, "%o", UINT_MAX);
    _PRINT_TEST((int)strlen(INT_OCTDIG) + 1, "0" INT_OCTDIG, "%#o", -1u);
#if 0
    /* TODO: This test case is broken, doesn't test what it was intended to. */
    _PRINT_TEST( 5, "%.0#o", "%.0#o", 0 );
#endif
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%+d", INT_MIN);
    _PRINT_TEST(INT_DIG + 1, "+" INT_MAX_DEZ_STR, "%+d", INT_MAX);
    _PRINT_TEST(2, "+0", "%+d", 0);
    _PRINT_TEST(UINT_DIG, UINT_MAX_DEZ_STR, "%+u", UINT_MAX);
    _PRINT_TEST(UINT_DIG, UINT_MAX_DEZ_STR, "%+u", -1u);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "% d", INT_MIN);
    _PRINT_TEST(INT_DIG + 1, " " INT_MAX_DEZ_STR, "% d", INT_MAX);
    _PRINT_TEST(2, " 0", "% d", 0);
    _PRINT_TEST(UINT_DIG, UINT_MAX_DEZ_STR, "% u", UINT_MAX);
    _PRINT_TEST(UINT_DIG, UINT_MAX_DEZ_STR, "% u", -1u);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%" INT_DIG_LESS1 "d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%" INT_DIG_LESS1 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%" sym2s(INT_DIG) "d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%" sym2s(INT_DIG) "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%" INT_DIG_PLUS1 "d", INT_MIN);
    _PRINT_TEST(INT_DIG + 1, " " INT_MAX_DEZ_STR, "%" INT_DIG_PLUS1 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 2, " -" INT_MIN_DEZ_STR, "%" INT_DIG_PLUS2 "d", INT_MIN);
    _PRINT_TEST(INT_DIG + 2, "  " INT_MAX_DEZ_STR, "%" INT_DIG_PLUS2 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%-" INT_DIG_LESS1 "d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%-" INT_DIG_LESS1 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%-" sym2s(INT_DIG) "d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%-" sym2s(INT_DIG) "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%-" INT_DIG_PLUS1 "d", INT_MIN);
    _PRINT_TEST(INT_DIG + 1, INT_MAX_DEZ_STR " ", "%-" INT_DIG_PLUS1 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 2, "-" INT_MIN_DEZ_STR " ", "%-" INT_DIG_PLUS2 "d", INT_MIN);
    _PRINT_TEST(INT_DIG + 2, INT_MAX_DEZ_STR "  ", "%-" INT_DIG_PLUS2 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%0" INT_DIG_LESS1 "d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%0" INT_DIG_LESS1 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%0" sym2s(INT_DIG) "d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%0" sym2s(INT_DIG) "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%0" INT_DIG_PLUS1 "d", INT_MIN);
    _PRINT_TEST(INT_DIG + 1, "0" INT_MAX_DEZ_STR, "%0" INT_DIG_PLUS1 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 2, "-0" INT_MIN_DEZ_STR, "%0" INT_DIG_PLUS2 "d", INT_MIN);
    _PRINT_TEST(INT_DIG + 2, "00" INT_MAX_DEZ_STR, "%0" INT_DIG_PLUS2 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%-0" INT_DIG_LESS1 "d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%-0" INT_DIG_LESS1 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%-0" sym2s(INT_DIG) "d", INT_MIN);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%-0" sym2s(INT_DIG) "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%-0" INT_DIG_PLUS1 "d", INT_MIN);
    _PRINT_TEST(INT_DIG + 1, INT_MAX_DEZ_STR " ", "%-0" INT_DIG_PLUS1 "d", INT_MAX);
    _PRINT_TEST(INT_DIG + 2, "-" INT_MIN_DEZ_STR " ", "%-0" INT_DIG_PLUS2 "d", INT_MIN);
    _PRINT_TEST(INT_DIG + 2, INT_MAX_DEZ_STR "  ", "%-0" INT_DIG_PLUS2 "d", INT_MAX);
    /* FIXME: This test not yet 32/64 bit agnostic */
    _PRINT_TEST(30, "          00000000002147483647", "%030.20d", INT_MAX);
    _PRINT_TEST((int)strlen(INT_HEXDIG) + 1, "f" INT_hexdig, "%.6x", UINT_MAX);
    _PRINT_TEST((int)strlen(INT_HEXDIG) + 3, "0xf" INT_hexdig, "%#6.3x", UINT_MAX);
    _PRINT_TEST((int)strlen(INT_HEXDIG) + 3, "0xf" INT_hexdig, "%#3.6x", UINT_MAX);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%.6d", INT_MIN);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%6.3d", INT_MIN);
    _PRINT_TEST(INT_DIG + 1, "-" INT_MIN_DEZ_STR, "%3.6d", INT_MIN);
    _PRINT_TEST(UINT_DIG, "0xf" INT_hexdig, "%#0.6x", UINT_MAX);
    _PRINT_TEST(UINT_DIG, "0xf" INT_hexdig, "%#06.3x", UINT_MAX);
    _PRINT_TEST(UINT_DIG, "0xf" INT_hexdig, "%#03.6x", UINT_MAX);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%#0.6d", INT_MAX);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%#06.3d", INT_MAX);
    _PRINT_TEST(INT_DIG, INT_MAX_DEZ_STR, "%#03.6d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "+" INT_MAX_DEZ_STR, "%#+.6d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "+" INT_MAX_DEZ_STR, "%#+6.3d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "+" INT_MAX_DEZ_STR, "%#+3.6d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "+" INT_MAX_DEZ_STR, "%+0.6d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "+" INT_MAX_DEZ_STR, "%+06.3d", INT_MAX);
    _PRINT_TEST(INT_DIG + 1, "+" INT_MAX_DEZ_STR, "%+03.6d", INT_MAX);
#ifndef TEST_CONVERSION_ONLY
    _PRINT_TEST(INT_DIG + 2, "- " INT_MAX_DEZ_STR, "- %d", INT_MAX);
    _PRINT_TEST(INT_DIG * 2 + 6, "- " INT_MAX_DEZ_STR " % -" INT_MIN_DEZ_STR, "- %d %% %d", INT_MAX, INT_MIN);
#endif
    _PRINT_TEST(1, "x", "%c", 'x');
    _PRINT_TEST(6, "abcdef", "%s", "abcdef");
    _PRINT_TEST(18, "0x00000000deadbeef", "%p", (void*)0xdeadbeef);
    _PRINT_TEST(6, "   0x1", "%#6x", 1);
#ifndef TEST_CONVERSION_ONLY
    {
        int val1, val2;
        _PRINT_TEST(9, "123456789", "123456%n789%n", &val1, &val2);
        TEST_ASSERT(val1 == 6);
        TEST_ASSERT(val2 == 9);
    }
#endif
    /* Verify "unusual" combinations of length and signedness */
    _PRINT_TEST(1, "1", "%tu", (ptrdiff_t)1);  /* unsigned prtdiff_t */
    _PRINT_TEST(2, "-1", "%jd", (intmax_t)-1); /* intmax_t */
    _PRINT_TEST(1, "1", "%ju", (uintmax_t)1);  /* uintmax_t */
    _PRINT_TEST(1, "1", "%zd", (size_t)1);     /* signed size_t */
    /******************************************************************************
     * NOTE: The following test cases are imported from the Tyndur project. They  *
     *       are therefore under the license of said project, not CC0.            *
     *       As said code comprises test cases, it does not form part of the      *
     *       final compiled library, and has no bearing on its licensing.         *
     ******************************************************************************/
    /*
     * Copyright (c) 2011 The tyndur Project. All rights reserved.
     *
     * This code is derived from software contributed to the tyndur Project
     * by Kevin Wolf.
     *
     * Redistribution and use in source and binary forms, with or without
     * modification, are permitted provided that the following conditions
     * are met:
     * 1. Redistributions of source code must retain the above copyright
     *    notice, this list of conditions and the following disclaimer.
     * 2. Redistributions in binary form must reproduce the above copyright
     *    notice, this list of conditions and the following disclaimer in the
     *    documentation and/or other materials provided with the distribution.
     *
     * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
     * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
     * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
     * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
     * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
     * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
     * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
     * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
     * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
     * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
     * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
     */
#ifndef TEST_CONVERSION_ONLY
    /* Ein String ohne alles */
    _PRINT_TEST(12, "Hallo heimur", "Hallo heimur");
#endif
    /* Einfache Konvertierungen */
    _PRINT_TEST(12, "Hallo heimur", "%s", "Hallo heimur");
    _PRINT_TEST(4, "1024", "%d", 1024);
    _PRINT_TEST(5, "-1024", "%d", -1024);
    _PRINT_TEST(4, "1024", "%i", 1024);
    _PRINT_TEST(5, "-1024", "%i", -1024);
    _PRINT_TEST(4, "1024", "%u", 1024u);
    _PRINT_TEST(10, "4294966272", "%u", -1024u);
    _PRINT_TEST(3, "777", "%o", 0777u);
    _PRINT_TEST(11, "37777777001", "%o", -0777u);
    _PRINT_TEST(8, "1234abcd", "%x", 0x1234abcdu);
    _PRINT_TEST(8, "edcb5433", "%x", -0x1234abcdu);
    _PRINT_TEST(8, "1234ABCD", "%X", 0x1234abcdu);
    _PRINT_TEST(8, "EDCB5433", "%X", -0x1234abcdu);
    _PRINT_TEST(1, "x", "%c", 'x');
    _PRINT_TEST(1, "%", "%%");
    /* Mit %c kann man auch Nullbytes ausgeben */
    _PRINT_TEST(1, "\0", "%c", '\0');
    /* Vorzeichen erzwingen (Flag +) */
    _PRINT_TEST(12, "Hallo heimur", "%+s", "Hallo heimur");
    _PRINT_TEST(5, "+1024", "%+d", 1024);
    _PRINT_TEST(5, "-1024", "%+d", -1024);
    _PRINT_TEST(5, "+1024", "%+i", 1024);
    _PRINT_TEST(5, "-1024", "%+i", -1024);
    _PRINT_TEST(4, "1024", "%+u", 1024u);
    _PRINT_TEST(10, "4294966272", "%+u", -1024u);
    _PRINT_TEST(3, "777", "%+o", 0777u);
    _PRINT_TEST(11, "37777777001", "%+o", -0777u);
    _PRINT_TEST(8, "1234abcd", "%+x", 0x1234abcdu);
    _PRINT_TEST(8, "edcb5433", "%+x", -0x1234abcdu);
    _PRINT_TEST(8, "1234ABCD", "%+X", 0x1234abcdu);
    _PRINT_TEST(8, "EDCB5433", "%+X", -0x1234abcdu);
    _PRINT_TEST(1, "x", "%+c", 'x');
    /* Vorzeichenplatzhalter erzwingen (Flag <space>) */
    _PRINT_TEST(12, "Hallo heimur", "% s", "Hallo heimur");
    _PRINT_TEST(5, " 1024", "% d", 1024);
    _PRINT_TEST(5, "-1024", "% d", -1024);
    _PRINT_TEST(5, " 1024", "% i", 1024);
    _PRINT_TEST(5, "-1024", "% i", -1024);
    _PRINT_TEST(4, "1024", "% u", 1024u);
    _PRINT_TEST(10, "4294966272", "% u", -1024u);
    _PRINT_TEST(3, "777", "% o", 0777u);
    _PRINT_TEST(11, "37777777001", "% o", -0777u);
    _PRINT_TEST(8, "1234abcd", "% x", 0x1234abcdu);
    _PRINT_TEST(8, "edcb5433", "% x", -0x1234abcdu);
    _PRINT_TEST(8, "1234ABCD", "% X", 0x1234abcdu);
    _PRINT_TEST(8, "EDCB5433", "% X", -0x1234abcdu);
    _PRINT_TEST(1, "x", "% c", 'x');
    /* Flag + hat Vorrang über <space> */
    _PRINT_TEST(12, "Hallo heimur", "%+ s", "Hallo heimur");
    _PRINT_TEST(5, "+1024", "%+ d", 1024);
    _PRINT_TEST(5, "-1024", "%+ d", -1024);
    _PRINT_TEST(5, "+1024", "%+ i", 1024);
    _PRINT_TEST(5, "-1024", "%+ i", -1024);
    _PRINT_TEST(4, "1024", "%+ u", 1024u);
    _PRINT_TEST(10, "4294966272", "%+ u", -1024u);
    _PRINT_TEST(3, "777", "%+ o", 0777u);
    _PRINT_TEST(11, "37777777001", "%+ o", -0777u);
    _PRINT_TEST(8, "1234abcd", "%+ x", 0x1234abcdu);
    _PRINT_TEST(8, "edcb5433", "%+ x", -0x1234abcdu);
    _PRINT_TEST(8, "1234ABCD", "%+ X", 0x1234abcdu);
    _PRINT_TEST(8, "EDCB5433", "%+ X", -0x1234abcdu);
    _PRINT_TEST(1, "x", "%+ c", 'x');
    /* Alternative Form */
    _PRINT_TEST(4, "0777", "%#o", 0777u);
    _PRINT_TEST(12, "037777777001", "%#o", -0777u);
    _PRINT_TEST(10, "0x1234abcd", "%#x", 0x1234abcdu);
    _PRINT_TEST(10, "0xedcb5433", "%#x", -0x1234abcdu);
    _PRINT_TEST(10, "0X1234ABCD", "%#X", 0x1234abcdu);
    _PRINT_TEST(10, "0XEDCB5433", "%#X", -0x1234abcdu);
    _PRINT_TEST(1, "0", "%#o", 0u);
    _PRINT_TEST(1, "0", "%#x", 0u);
    _PRINT_TEST(1, "0", "%#X", 0u);
    /* Feldbreite: Kleiner als Ausgabe */
    _PRINT_TEST(12, "Hallo heimur", "%1s", "Hallo heimur");
    _PRINT_TEST(4, "1024", "%1d", 1024);
    _PRINT_TEST(5, "-1024", "%1d", -1024);
    _PRINT_TEST(4, "1024", "%1i", 1024);
    _PRINT_TEST(5, "-1024", "%1i", -1024);
    _PRINT_TEST(4, "1024", "%1u", 1024u);
    _PRINT_TEST(10, "4294966272", "%1u", -1024u);
    _PRINT_TEST(3, "777", "%1o", 0777u);
    _PRINT_TEST(11, "37777777001", "%1o", -0777u);
    _PRINT_TEST(8, "1234abcd", "%1x", 0x1234abcdu);
    _PRINT_TEST(8, "edcb5433", "%1x", -0x1234abcdu);
    _PRINT_TEST(8, "1234ABCD", "%1X", 0x1234abcdu);
    _PRINT_TEST(8, "EDCB5433", "%1X", -0x1234abcdu);
    _PRINT_TEST(1, "x", "%1c", 'x');
    /* Feldbreite: Größer als Ausgabe */
    _PRINT_TEST(20, "               Hallo", "%20s", "Hallo");
    _PRINT_TEST(20, "                1024", "%20d", 1024);
    _PRINT_TEST(20, "               -1024", "%20d", -1024);
    _PRINT_TEST(20, "                1024", "%20i", 1024);
    _PRINT_TEST(20, "               -1024", "%20i", -1024);
    _PRINT_TEST(20, "                1024", "%20u", 1024u);
    _PRINT_TEST(20, "          4294966272", "%20u", -1024u);
    _PRINT_TEST(20, "                 777", "%20o", 0777u);
    _PRINT_TEST(20, "         37777777001", "%20o", -0777u);
    _PRINT_TEST(20, "            1234abcd", "%20x", 0x1234abcdu);
    _PRINT_TEST(20, "            edcb5433", "%20x", -0x1234abcdu);
    _PRINT_TEST(20, "            1234ABCD", "%20X", 0x1234abcdu);
    _PRINT_TEST(20, "            EDCB5433", "%20X", -0x1234abcdu);
    _PRINT_TEST(20, "                   x", "%20c", 'x');
    /* Feldbreite: Linksbündig */
    _PRINT_TEST(20, "Hallo               ", "%-20s", "Hallo");
    _PRINT_TEST(20, "1024                ", "%-20d", 1024);
    _PRINT_TEST(20, "-1024               ", "%-20d", -1024);
    _PRINT_TEST(20, "1024                ", "%-20i", 1024);
    _PRINT_TEST(20, "-1024               ", "%-20i", -1024);
    _PRINT_TEST(20, "1024                ", "%-20u", 1024u);
    _PRINT_TEST(20, "4294966272          ", "%-20u", -1024u);
    _PRINT_TEST(20, "777                 ", "%-20o", 0777u);
    _PRINT_TEST(20, "37777777001         ", "%-20o", -0777u);
    _PRINT_TEST(20, "1234abcd            ", "%-20x", 0x1234abcdu);
    _PRINT_TEST(20, "edcb5433            ", "%-20x", -0x1234abcdu);
    _PRINT_TEST(20, "1234ABCD            ", "%-20X", 0x1234abcdu);
    _PRINT_TEST(20, "EDCB5433            ", "%-20X", -0x1234abcdu);
    _PRINT_TEST(20, "x                   ", "%-20c", 'x');
    /* Feldbreite: Padding mit 0 */
    _PRINT_TEST(20, "00000000000000001024", "%020d", 1024);
    _PRINT_TEST(20, "-0000000000000001024", "%020d", -1024);
    _PRINT_TEST(20, "00000000000000001024", "%020i", 1024);
    _PRINT_TEST(20, "-0000000000000001024", "%020i", -1024);
    _PRINT_TEST(20, "00000000000000001024", "%020u", 1024u);
    _PRINT_TEST(20, "00000000004294966272", "%020u", -1024u);
    _PRINT_TEST(20, "00000000000000000777", "%020o", 0777u);
    _PRINT_TEST(20, "00000000037777777001", "%020o", -0777u);
    _PRINT_TEST(20, "0000000000001234abcd", "%020x", 0x1234abcdu);
    _PRINT_TEST(20, "000000000000edcb5433", "%020x", -0x1234abcdu);
    _PRINT_TEST(20, "0000000000001234ABCD", "%020X", 0x1234abcdu);
    _PRINT_TEST(20, "000000000000EDCB5433", "%020X", -0x1234abcdu);
    /* Feldbreite: Padding und alternative Form */
    _PRINT_TEST(20, "                0777", "%#20o", 0777u);
    _PRINT_TEST(20, "        037777777001", "%#20o", -0777u);
    _PRINT_TEST(20, "          0x1234abcd", "%#20x", 0x1234abcdu);
    _PRINT_TEST(20, "          0xedcb5433", "%#20x", -0x1234abcdu);
    _PRINT_TEST(20, "          0X1234ABCD", "%#20X", 0x1234abcdu);
    _PRINT_TEST(20, "          0XEDCB5433", "%#20X", -0x1234abcdu);
    _PRINT_TEST(20, "00000000000000000777", "%#020o", 0777u);
    _PRINT_TEST(20, "00000000037777777001", "%#020o", -0777u);
    _PRINT_TEST(20, "0x00000000001234abcd", "%#020x", 0x1234abcdu);
    _PRINT_TEST(20, "0x0000000000edcb5433", "%#020x", -0x1234abcdu);
    _PRINT_TEST(20, "0X00000000001234ABCD", "%#020X", 0x1234abcdu);
    _PRINT_TEST(20, "0X0000000000EDCB5433", "%#020X", -0x1234abcdu);
    /* Feldbreite: - hat Vorrang vor 0 */
    _PRINT_TEST(20, "Hallo               ", "%0-20s", "Hallo");
    _PRINT_TEST(20, "1024                ", "%0-20d", 1024);
    _PRINT_TEST(20, "-1024               ", "%0-20d", -1024);
    _PRINT_TEST(20, "1024                ", "%0-20i", 1024);
    _PRINT_TEST(20, "-1024               ", "%0-20i", -1024);
    _PRINT_TEST(20, "1024                ", "%0-20u", 1024u);
    _PRINT_TEST(20, "4294966272          ", "%0-20u", -1024u);
    _PRINT_TEST(20, "777                 ", "%-020o", 0777u);
    _PRINT_TEST(20, "37777777001         ", "%-020o", -0777u);
    _PRINT_TEST(20, "1234abcd            ", "%-020x", 0x1234abcdu);
    _PRINT_TEST(20, "edcb5433            ", "%-020x", -0x1234abcdu);
    _PRINT_TEST(20, "1234ABCD            ", "%-020X", 0x1234abcdu);
    _PRINT_TEST(20, "EDCB5433            ", "%-020X", -0x1234abcdu);
    _PRINT_TEST(20, "x                   ", "%-020c", 'x');
    /* Feldbreite: Aus Parameter */
    _PRINT_TEST(20, "               Hallo", "%*s", 20, "Hallo");
    _PRINT_TEST(20, "                1024", "%*d", 20, 1024);
    _PRINT_TEST(20, "               -1024", "%*d", 20, -1024);
    _PRINT_TEST(20, "                1024", "%*i", 20, 1024);
    _PRINT_TEST(20, "               -1024", "%*i", 20, -1024);
    _PRINT_TEST(20, "                1024", "%*u", 20, 1024u);
    _PRINT_TEST(20, "          4294966272", "%*u", 20, -1024u);
    _PRINT_TEST(20, "                 777", "%*o", 20, 0777u);
    _PRINT_TEST(20, "         37777777001", "%*o", 20, -0777u);
    _PRINT_TEST(20, "            1234abcd", "%*x", 20, 0x1234abcdu);
    _PRINT_TEST(20, "            edcb5433", "%*x", 20, -0x1234abcdu);
    _PRINT_TEST(20, "            1234ABCD", "%*X", 20, 0x1234abcdu);
    _PRINT_TEST(20, "            EDCB5433", "%*X", 20, -0x1234abcdu);
    _PRINT_TEST(20, "                   x", "%*c", 20, 'x');
    /* Präzision / Mindestanzahl von Ziffern */
    _PRINT_TEST(12, "Hallo heimur", "%.20s", "Hallo heimur");
    _PRINT_TEST(20, "00000000000000001024", "%.20d", 1024);
    _PRINT_TEST(21, "-00000000000000001024", "%.20d", -1024);
    _PRINT_TEST(20, "00000000000000001024", "%.20i", 1024);
    _PRINT_TEST(21, "-00000000000000001024", "%.20i", -1024);
    _PRINT_TEST(20, "00000000000000001024", "%.20u", 1024u);
    _PRINT_TEST(20, "00000000004294966272", "%.20u", -1024u);
    _PRINT_TEST(20, "00000000000000000777", "%.20o", 0777u);
    _PRINT_TEST(20, "00000000037777777001", "%.20o", -0777u);
    _PRINT_TEST(20, "0000000000001234abcd", "%.20x", 0x1234abcdu);
    _PRINT_TEST(20, "000000000000edcb5433", "%.20x", -0x1234abcdu);
    _PRINT_TEST(20, "0000000000001234ABCD", "%.20X", 0x1234abcdu);
    _PRINT_TEST(20, "000000000000EDCB5433", "%.20X", -0x1234abcdu);
    /* Feldbreite und Präzision */
    _PRINT_TEST(20, "               Hallo", "%20.5s", "Hallo heimur");
    _PRINT_TEST(20, "               01024", "%20.5d", 1024);
    _PRINT_TEST(20, "              -01024", "%20.5d", -1024);
    _PRINT_TEST(20, "               01024", "%20.5i", 1024);
    _PRINT_TEST(20, "              -01024", "%20.5i", -1024);
    _PRINT_TEST(20, "               01024", "%20.5u", 1024u);
    _PRINT_TEST(20, "          4294966272", "%20.5u", -1024u);
    _PRINT_TEST(20, "               00777", "%20.5o", 0777u);
    _PRINT_TEST(20, "         37777777001", "%20.5o", -0777u);
    _PRINT_TEST(20, "            1234abcd", "%20.5x", 0x1234abcdu);
    _PRINT_TEST(20, "          00edcb5433", "%20.10x", -0x1234abcdu);
    _PRINT_TEST(20, "            1234ABCD", "%20.5X", 0x1234abcdu);
    _PRINT_TEST(20, "          00EDCB5433", "%20.10X", -0x1234abcdu);
    /* Präzision: 0 wird ignoriert */
    _PRINT_TEST(20, "               01024", "%020.5d", 1024);
    _PRINT_TEST(20, "              -01024", "%020.5d", -1024);
    _PRINT_TEST(20, "               01024", "%020.5i", 1024);
    _PRINT_TEST(20, "              -01024", "%020.5i", -1024);
    _PRINT_TEST(20, "               01024", "%020.5u", 1024u);
    _PRINT_TEST(20, "          4294966272", "%020.5u", -1024u);
    _PRINT_TEST(20, "               00777", "%020.5o", 0777u);
    _PRINT_TEST(20, "         37777777001", "%020.5o", -0777u);
    _PRINT_TEST(20, "            1234abcd", "%020.5x", 0x1234abcdu);
    _PRINT_TEST(20, "          00edcb5433", "%020.10x", -0x1234abcdu);
    _PRINT_TEST(20, "            1234ABCD", "%020.5X", 0x1234abcdu);
    _PRINT_TEST(20, "          00EDCB5433", "%020.10X", -0x1234abcdu);
    /* Präzision 0 */
    _PRINT_TEST(0, "", "%.0s", "Hallo heimur");
    _PRINT_TEST(20, "                    ", "%20.0s", "Hallo heimur");
    _PRINT_TEST(0, "", "%.s", "Hallo heimur");
    _PRINT_TEST(20, "                    ", "%20.s", "Hallo heimur");
    _PRINT_TEST(20, "                1024", "%20.0d", 1024);
    _PRINT_TEST(20, "               -1024", "%20.d", -1024);
    _PRINT_TEST(20, "                    ", "%20.d", 0);
    _PRINT_TEST(20, "                1024", "%20.0i", 1024);
    _PRINT_TEST(20, "               -1024", "%20.i", -1024);
    _PRINT_TEST(20, "                    ", "%20.i", 0);
    _PRINT_TEST(20, "                1024", "%20.u", 1024u);
    _PRINT_TEST(20, "          4294966272", "%20.0u", -1024u);
    _PRINT_TEST(20, "                    ", "%20.u", 0u);
    _PRINT_TEST(20, "                 777", "%20.o", 0777u);
    _PRINT_TEST(20, "         37777777001", "%20.0o", -0777u);
    _PRINT_TEST(20, "                    ", "%20.o", 0u);
    _PRINT_TEST(20, "            1234abcd", "%20.x", 0x1234abcdu);
    _PRINT_TEST(20, "            edcb5433", "%20.0x", -0x1234abcdu);
    _PRINT_TEST(20, "                    ", "%20.x", 0u);
    _PRINT_TEST(20, "            1234ABCD", "%20.X", 0x1234abcdu);
    _PRINT_TEST(20, "            EDCB5433", "%20.0X", -0x1234abcdu);
    _PRINT_TEST(20, "                    ", "%20.X", 0u);
    /* Negative Präzision wird ignoriert */
    /* XXX glibc tut nicht, was ich erwartet habe, vorerst deaktiviert... */
    /*
     * Präzision und Feldbreite aus Parameter.
     * + hat Vorrang vor <space>, - hat Vorrang vor 0 (das eh ignoriert wird,
     * weil eine Präzision angegeben ist)
     */
    _PRINT_TEST(20, "Hallo               ", "% -0+*.*s", 20, 5, "Hallo heimur");
    _PRINT_TEST(20, "+01024              ", "% -0+*.*d", 20, 5, 1024);
    _PRINT_TEST(20, "-01024              ", "% -0+*.*d", 20, 5, -1024);
    _PRINT_TEST(20, "+01024              ", "% -0+*.*i", 20, 5, 1024);
    _PRINT_TEST(20, "-01024              ", "% 0-+*.*i", 20, 5, -1024);
    _PRINT_TEST(20, "01024               ", "% 0-+*.*u", 20, 5, 1024u);
    _PRINT_TEST(20, "4294966272          ", "% 0-+*.*u", 20, 5, -1024u);
    _PRINT_TEST(20, "00777               ", "%+ -0*.*o", 20, 5, 0777u);
    _PRINT_TEST(20, "37777777001         ", "%+ -0*.*o", 20, 5, -0777u);
    _PRINT_TEST(20, "1234abcd            ", "%+ -0*.*x", 20, 5, 0x1234abcdu);
    _PRINT_TEST(20, "00edcb5433          ", "%+ -0*.*x", 20, 10, -0x1234abcdu);
    _PRINT_TEST(20, "1234ABCD            ", "% -+0*.*X", 20, 5, 0x1234abcdu);
    _PRINT_TEST(20, "00EDCB5433          ", "% -+0*.*X", 20, 10, -0x1234abcdu);
#if __GNUC__
#pragma GCC diagnostic pop
#endif

    return 0;
}
/******************************************************************************/

TEST_DEFINE(print)
{
    for (int k = 0; k < 1; ++k)
    {
        TEST_ASSERT(_test_print_iter() != ERR);
    }

    return 0;
}

#endif
#endif
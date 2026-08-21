/* SPDX-License-Identifier: BSD-2-Clause */

#include "ttest/test.h"

#include <string.h>

void check_boolean(const char *file, int line, bool isFatal,
        bool expected, bool actual, const char* expression) {
    if (expected == actual) {
        return;
    }
    ttest_mark_current_case_as_failed(file, line, isFatal,
            "<%s> is not <%s>", expression, expected ? "true" : "false");
}

#define DEFINE_CHECK_EQUAL_FOR_INTEGRAL(type, suffix, formatSpec) \
void check_equal_ ## suffix(const char *file, int line, bool isFatal, bool invert, \
        type expected, type actual) { \
    const bool eq = expected == actual; \
    if ((!invert && eq) || (invert && !eq)) { \
        return; \
    } \
    ttest_mark_current_case_as_failed(file, line, isFatal, \
            "%sexpected <" formatSpec "> but got <" formatSpec ">", \
            invert ? "not " : "", \
            expected, actual); \
}

DEFINE_CHECK_EQUAL_FOR_INTEGRAL(char, char, "%c")
DEFINE_CHECK_EQUAL_FOR_INTEGRAL(signed int, sint, "%d")
DEFINE_CHECK_EQUAL_FOR_INTEGRAL(unsigned int, uint, "%u")
DEFINE_CHECK_EQUAL_FOR_INTEGRAL(signed long, slong, "%ld")
DEFINE_CHECK_EQUAL_FOR_INTEGRAL(unsigned long, ulong, "%lu")

void check_equal_cstr(const char *file, int line, bool isFatal, bool invert,
        struct cstr expected, struct cstr actual) {
    const bool eq = strcmp(expected.data, actual.data) == 0;
    if ((!invert && eq) || (invert && !eq)) {
        return;
    }
    ttest_mark_current_case_as_failed(file, line, isFatal, "%sexpected <%s> but got <%s>",
            invert ? "not " : "",
            expected.data, actual.data);
}

void check_equal_span(const char *file, int line, bool isFatal, bool invert,
        struct span expected, struct span actual) {
    const bool sameLen = expected.sz == actual.sz;
    int firstDifferent = -1;
    const unsigned char * const p = expected.data;
    const unsigned char * const q = actual.data;
    if (sameLen) {
        for (unsigned i = 0; i < expected.sz; i++) {
            if (p[i] != q[i]) {
                firstDifferent = i;
                break;
            }
        }
    }

    const bool eq = sameLen && firstDifferent == -1;

    if (invert) {
        if (!eq) {
            return;
        }
        ttest_mark_current_case_as_failed(file, line, isFatal,
                "got identical %u-byte length spans", expected.sz);
    } else {
        if (eq) {
            return;
        }
        if (sameLen) {
            ttest_mark_current_case_as_failed(file, line, isFatal,
                    "spans differ starting at byte [%d]: expected <%02x> but got <%02x>",
                    firstDifferent, p[firstDifferent], q[firstDifferent]);
        } else {
            ttest_mark_current_case_as_failed(file, line, isFatal,
                    "expected %u-byte length span but got %u-byte length", expected.sz, actual.sz);
        }
    }
}


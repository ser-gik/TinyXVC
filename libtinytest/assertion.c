/*
 * Copyright 2021 Sergey Guralnik
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ttest/test.h"

#include <string.h>
#include <stdio.h>

void check_boolean(const char *file, int line, bool isFatal,
        bool expected, bool actual, const char* expression) {
    if (expected == actual) {
        return;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "<%s> is not <%s>", expression, expected ? "true" : "false");
    ttest_mark_current_case_as_failed(file, line, msg, isFatal);
}

#define DEFINE_CHECK_EQUAL_FOR_INTEGRAL(type, suffix, formatSpec) \
void check_equal_ ## suffix(const char *file, int line, bool isFatal, bool invert, \
        type expected, type actual) { \
    const bool eq = expected == actual; \
    if ((!invert && eq) || (invert && !eq)) { \
        return; \
    } \
    char msg[128]; \
    snprintf(msg, sizeof(msg), "%sexpected <" formatSpec "> but got <" formatSpec ">", \
            invert ? "not " : "", \
            expected, actual); \
    ttest_mark_current_case_as_failed(file, line, msg, isFatal); \
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
    char msg[128];
    snprintf(msg, sizeof(msg), "%sexpected <%s> but got <%s>",
            invert ? "not " : "",
            expected.data, actual.data);
    ttest_mark_current_case_as_failed(file, line, msg, isFatal);
}


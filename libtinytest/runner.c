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
#include "ttest/test_private.h"

#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

struct strbuf {
    char str[4096];
    size_t sz;
};

static void strbuf_reset(struct strbuf *sb) {
    sb->str[0] = '\0';
    sb->sz = 0u;
}

static void strbuf_vappend(struct strbuf *sb, const char *format, va_list ap) {
    if (sizeof(sb->str) <= sb->sz + 1u) {
        return;
    }
    size_t avail = sizeof(sb->str) - sb->sz;
    int res = vsnprintf(sb->str + sb->sz, avail, format, ap);
    if (res < 0) {
        return;
    }
    size_t len = (size_t)res;
    sb->sz += len > avail ? avail : len;
}

static void strbuf_append(struct strbuf *sb, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    strbuf_vappend(sb, format, ap);
    va_end(ap);
}

#define MAX_SUITES 100

struct test {
    struct test_suite *suites[MAX_SUITES];
    int numSuites;
    int numFailedCases;
    int numCasesTotal;
} gTinyTest = {
    .numSuites = 0,
    .numFailedCases = 0,
    .numCasesTotal = 0,
};

static struct test_context {
    jmp_buf restorePoint;
    bool failed;
    struct strbuf messages;
} gTestCaseContext;

static void run_case(struct test_suite *suite, struct test_case *case_) {
    /* Use volatile version of locals to be sure that setjmp/longjmp won't clobber them */
    struct test_suite * volatile vSuite = suite;
    struct test_case * volatile vCase = case_;
    struct test_context * volatile vCtx = &gTestCaseContext;

    gTinyTest.numCasesTotal++;
    printf("[%s] [%s] - ", vSuite->name, vCase->name);
    vCtx->failed = false;
    strbuf_reset(&vCtx->messages);

    if (setjmp(vCtx->restorePoint) == 0) {
        if (vSuite->beforeCaseFn) {
            vSuite->beforeCaseFn();
        }
        if (setjmp(vCtx->restorePoint) == 0) {
            vCase->testFn();
        }
        if (setjmp(vCtx->restorePoint) == 0) {
            if (vSuite->afterCaseFn) {
                vSuite->afterCaseFn();
            }
        }
    }

    if (vCtx->failed) {
        printf("FAILED\n");
        fputs(vCtx->messages.str, stdout);
        gTinyTest.numFailedCases++;
    } else {
        printf("OK\n");
    }
    printf("\n");
}

void ttest_private_register_suite(struct test_suite *suite) {
    if (gTinyTest.numSuites < MAX_SUITES) {
        gTinyTest.suites[gTinyTest.numSuites++] = suite;
    } else {
        ttest_private_abort("Too many suites");
    }
}

void ttest_private_register_case(struct test_suite *suite, struct test_case *case_) {
    if (suite->numCases < MAX_CASES_PER_SUITE) {
        suite->cases[suite->numCases++] = case_;
    } else {
        ttest_private_abort("Too many cases");
    }
}

void ttest_private_abort(const char* message) {
    fprintf(stderr, "%s\n", message);
    abort();
}

void ttest_mark_current_case_as_failed(const char *file, int line, bool isFatal,
        const char* format, ...) {
    struct test_context *ctx = &gTestCaseContext;
    ctx->failed = true;
    strbuf_append(&ctx->messages, "%s:%d: ", file, line);
    {
        va_list ap;
        va_start(ap, format);
        strbuf_vappend(&ctx->messages, format, ap);
        va_end(ap);
    }
    strbuf_append(&ctx->messages, "\n");
    if (isFatal) {
        longjmp(gTestCaseContext.restorePoint, 1);
    }
}

bool ttest_run_all(void) {
    for (int suiteIdx = 0; suiteIdx < gTinyTest.numSuites; suiteIdx++) {
        struct test_suite *suite = gTinyTest.suites[suiteIdx];
        for (int caseIdx = 0; caseIdx < suite->numCases; caseIdx++) {
            struct test_case *case_ = suite->cases[caseIdx];
            run_case(suite, case_);
        }
    }
    printf("Total: %d, failed: %d\n", gTinyTest.numCasesTotal, gTinyTest.numFailedCases);
    return gTinyTest.numFailedCases == 0;
}


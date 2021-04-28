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

#include "test.h"

#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

struct test gTest = {
    .numSuites = 0,
    .numFailedCases = 0,
    .numCasesTotal = 0,
};

#define MAX_FAILED_TEST_MESSAGE 4096

static struct test_context {
    jmp_buf restorePoint;
    bool failed;
    char messages[MAX_FAILED_TEST_MESSAGE];
    int messagesHead;
} gTestCaseContext;

static void run_case(struct test_suite *suite, struct test_case *case_) {
    gTest.numCasesTotal++;
    printf("[%s] [%s] - ", suite->name, case_->name);
    {
        struct test_context *ctx = &gTestCaseContext;
        ctx->failed = false;
        ctx->messages[0] = '\0';
        ctx->messagesHead = 0;
    }
    if (setjmp(gTestCaseContext.restorePoint) == 0) {
        case_->testFn();
    }
    {
        struct test_context *ctx = &gTestCaseContext;
        if (ctx->failed) {
            printf("FAILED\n");
            fputs(ctx->messages, stdout);
            gTest.numFailedCases++;
        } else {
            printf("OK\n");
        }
    }
    printf("\n");
}

void test_mark_current_as_failed(const char *file, int line, const char* message, bool isFatal) {
    struct test_context *ctx = &gTestCaseContext;
    ctx->failed = true;
    int avail = MAX_FAILED_TEST_MESSAGE - ctx->messagesHead - 1;
    if (avail > 0) {
        int msgLen = snprintf(ctx->messages + ctx->messagesHead, avail,
                "%s:%d: %s\n", file, line, message);
        ctx->messagesHead += msgLen;
        if (ctx->messagesHead > MAX_FAILED_TEST_MESSAGE) {
            ctx->messagesHead = MAX_FAILED_TEST_MESSAGE;
        }
    }
    if (isFatal) {
        longjmp(gTestCaseContext.restorePoint, 1);
    }
}

bool test_run_all(void) {
    for (int suiteIdx = 0; suiteIdx < gTest.numSuites; suiteIdx++) {
        struct test_suite *suite = gTest.suites[suiteIdx];
        for (int caseIdx = 0; caseIdx < suite->numCases; caseIdx++) {
            struct test_case *case_ = suite->cases[caseIdx];
            run_case(suite, case_);
        }
    }
    printf("Total: %d, failed: %d\n", gTest.numCasesTotal, gTest.numFailedCases);
    return gTest.numFailedCases == 0;
}


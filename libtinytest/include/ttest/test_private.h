/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

struct test_case {
    const char *name;
    void (*testFn)(void);
};

#define MAX_CASES_PER_SUITE 100

struct test_suite {
    const char *name;
    struct test_case *cases[MAX_CASES_PER_SUITE];
    int numCases;
    void (*beforeCaseFn)(void);
    void (*afterCaseFn)(void);
};

void ttest_private_register_suite(struct test_suite *suite);
void ttest_private_register_case(struct test_suite *suite, struct test_case *case_);
void ttest_private_abort(const char* message);
void ttest_noop(void);

#ifdef __GNUC__
#define ATTR_GLOBAL_CTOR __attribute__((constructor))
#else
#error Do not know how to declare global constructor function
#endif


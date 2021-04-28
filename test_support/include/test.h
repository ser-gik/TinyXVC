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

#pragma once

#include "test_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * Defining tests and test suites.
 */

/**
 * Defines test suite.
 * There MUST be exactly one test suite per translation unit.
 */
#define TEST_SUITE(suiteName) \
    static struct test_suite gTestSuite = { \
        .name = #suiteName, \
        .numCases = 0, \
    }; \
    ATTR_GLOBAL_CTOR static void registerSuite(void) { \
        if (gTest.numSuites < MAX_SUITES) gTest.suites[gTest.numSuites++] = &gTestSuite; \
        else { fprintf(stderr, "Too many suites\n"); abort(); } \
    }

/**
 * Defines a test case in the current test suite.
 * E.g.:
 *     TEST_CASE(add) {
 *         ASSERT_EQ(4, 2 + 2);
 *     }
 */
#define TEST_CASE(caseName) \
    static void caseName(void); \
    static struct test_case gTestCase_ ## caseName = {\
        .name = #caseName, \
        .testFn = caseName, \
    }; \
    ATTR_GLOBAL_CTOR static void registerCase_ ## caseName(void) { \
        if (gTestSuite.numCases < MAX_CASES_PER_SUITE) \
            gTestSuite.cases[gTestSuite.numCases++] = &gTestCase_ ## caseName; \
        else { fprintf(stderr, "Too many cases\n"); abort(); } \
    } \
    static void caseName(void)

/**
 * Runner interface
 */
void test_mark_current_as_failed(const char *file, int line, const char* message, bool isFatal);
bool test_run_all(void);

/**
 * Assertions
 */
void check_boolean(const char *file, int line, bool isFatal,
        bool expected, bool actual, const char* expression);

#define CHECK_BOOLEAN(expr, expected, isFatal) check_boolean(__FILE__, __LINE__, isFatal, \
        expected, (expr), #expr)
#define ASSERT_TRUE(expr) CHECK_BOOLEAN(expr, true, true)
#define EXPECT_TRUE(expr) CHECK_BOOLEAN(expr, true, false)
#define ASSERT_FALSE(expr) CHECK_BOOLEAN(expr, false, true)
#define EXPECT_FALSE(expr) CHECK_BOOLEAN(expr, false, false)

void check_equal_char(const char *file, int line, bool isFatal, bool invert,
        char expected, char actual);
void check_equal_sint(const char *file, int line, bool isFatal, bool invert,
        signed int expected, signed int actual);
void check_equal_uint(const char *file, int line, bool isFatal, bool invert,
        unsigned int expected, unsigned int actual);
void check_equal_slong(const char *file, int line, bool isFatal, bool invert,
        signed long expected, signed long actual);
void check_equal_ulong(const char *file, int line, bool isFatal, bool invert,
        unsigned long expected, unsigned long actual);

struct cstr { const char *data; };
#define CSTR(ptr) (struct cstr){ .data = ptr }
void check_equal_cstr(const char *file, int line, bool isFatal, bool invert,
        struct cstr expected, struct cstr actual);

#define CHECK_EQUAL(expected, actual, isFatal, invert) \
    _Generic((expected), \
            char: check_equal_char, \
            signed int: check_equal_sint, \
            unsigned int: check_equal_uint, \
            signed long: check_equal_slong, \
            unsigned long: check_equal_ulong, \
            struct cstr: check_equal_cstr) \
            (__FILE__, __LINE__, isFatal, invert, expected, actual)
#define ASSERT_EQ(expected, actual) CHECK_EQUAL(expected, actual, true, false)
#define EXPECT_EQ(expected, actual) CHECK_EQUAL(expected, actual, false, false)
#define ASSERT_NE(expected, actual) CHECK_EQUAL(expected, actual, true, true)
#define EXPECT_NE(expected, actual) CHECK_EQUAL(expected, actual, false, true)


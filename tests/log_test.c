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

#include "txvc/log.h"

TEST_SUITE(Logger)

DO_AFTER_EACH_CASE() {
    txvc_log_configure("all+", LOG_LEVEL_ERROR);
}

TEST_CASE(MinimalLogLevelIsSet_AllBelowItAreDisabled) {
    txvc_log_configure("", LOG_LEVEL_FATAL);
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_VERBOSE));
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_INFO));
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_WARN));
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_ERROR));
    EXPECT_TRUE(txvc_log_level_enabled(LOG_LEVEL_FATAL));

    txvc_log_configure("", LOG_LEVEL_WARN);
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_VERBOSE));
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_INFO));
    EXPECT_TRUE(txvc_log_level_enabled(LOG_LEVEL_WARN));
    EXPECT_TRUE(txvc_log_level_enabled(LOG_LEVEL_ERROR));
    EXPECT_TRUE(txvc_log_level_enabled(LOG_LEVEL_FATAL));
}

TEST_CASE(TagSpecIsProvided_TagsAreEnabledAsPerSpec) {
#define IS_TAG_ENABLED(name) \
    txvc_log_tag_enabled(&(struct txvc_log_tag) TXVC_LOG_TAG_INITIALIZER(name))

    txvc_log_configure("all-", LOG_LEVEL_VERBOSE);
    EXPECT_FALSE(IS_TAG_ENABLED(foo));
    EXPECT_FALSE(IS_TAG_ENABLED(bar));
    EXPECT_FALSE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all-baz+", LOG_LEVEL_VERBOSE);
    EXPECT_FALSE(IS_TAG_ENABLED(foo));
    EXPECT_FALSE(IS_TAG_ENABLED(bar));
    EXPECT_TRUE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all-foo+foo-", LOG_LEVEL_VERBOSE);
    EXPECT_FALSE(IS_TAG_ENABLED(foo));
    EXPECT_FALSE(IS_TAG_ENABLED(bar));
    EXPECT_FALSE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all+", LOG_LEVEL_VERBOSE);
    EXPECT_TRUE(IS_TAG_ENABLED(foo));
    EXPECT_TRUE(IS_TAG_ENABLED(bar));
    EXPECT_TRUE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all+bar-bar+", LOG_LEVEL_VERBOSE);
    EXPECT_TRUE(IS_TAG_ENABLED(foo));
    EXPECT_TRUE(IS_TAG_ENABLED(bar));
    EXPECT_TRUE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all+baz-", LOG_LEVEL_VERBOSE);
    EXPECT_TRUE(IS_TAG_ENABLED(foo));
    EXPECT_TRUE(IS_TAG_ENABLED(bar));
    EXPECT_FALSE(IS_TAG_ENABLED(baz));

#undef IS_TAG_ENABLED
}

TEST_CASE(EnableTagThenDisable_TagStateFollowsTheMostRecentConfig) {
    struct txvc_log_tag tag = TXVC_LOG_TAG_INITIALIZER(foo);
    txvc_log_configure("foo+", LOG_LEVEL_VERBOSE);
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    txvc_log_configure("foo-", LOG_LEVEL_VERBOSE);
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    txvc_log_configure("foo+", LOG_LEVEL_VERBOSE);
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    txvc_log_configure("foo-", LOG_LEVEL_VERBOSE);
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
}


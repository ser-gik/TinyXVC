/* SPDX-License-Identifier: BSD-2-Clause */

#include "ttest/test.h"

#include "txvc/log.h"

TEST_SUITE(Logger)

DO_AFTER_EACH_CASE() {
    txvc_log_configure("all+", LOG_LEVEL_ERROR, false);
}

TEST_CASE(MinimalLogLevelIsSet_AllBelowItAreDisabled) {
    txvc_log_configure("", LOG_LEVEL_FATAL, false);
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_VERBOSE));
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_INFO));
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_WARN));
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_ERROR));
    EXPECT_TRUE(txvc_log_level_enabled(LOG_LEVEL_FATAL));

    txvc_log_configure("", LOG_LEVEL_WARN, false);
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_VERBOSE));
    EXPECT_FALSE(txvc_log_level_enabled(LOG_LEVEL_INFO));
    EXPECT_TRUE(txvc_log_level_enabled(LOG_LEVEL_WARN));
    EXPECT_TRUE(txvc_log_level_enabled(LOG_LEVEL_ERROR));
    EXPECT_TRUE(txvc_log_level_enabled(LOG_LEVEL_FATAL));
}

TEST_CASE(TagSpecIsProvided_TagsAreEnabledAsPerSpec) {
#define IS_TAG_ENABLED(name) \
    txvc_log_tag_enabled(&(struct txvc_log_tag) TXVC_LOG_TAG_INITIALIZER(name))

    txvc_log_configure("all-", LOG_LEVEL_VERBOSE, false);
    EXPECT_FALSE(IS_TAG_ENABLED(foo));
    EXPECT_FALSE(IS_TAG_ENABLED(bar));
    EXPECT_FALSE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all-baz+", LOG_LEVEL_VERBOSE, false);
    EXPECT_FALSE(IS_TAG_ENABLED(foo));
    EXPECT_FALSE(IS_TAG_ENABLED(bar));
    EXPECT_TRUE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all-foo+foo-", LOG_LEVEL_VERBOSE, false);
    EXPECT_FALSE(IS_TAG_ENABLED(foo));
    EXPECT_FALSE(IS_TAG_ENABLED(bar));
    EXPECT_FALSE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all+", LOG_LEVEL_VERBOSE, false);
    EXPECT_TRUE(IS_TAG_ENABLED(foo));
    EXPECT_TRUE(IS_TAG_ENABLED(bar));
    EXPECT_TRUE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all+bar-bar+", LOG_LEVEL_VERBOSE, false);
    EXPECT_TRUE(IS_TAG_ENABLED(foo));
    EXPECT_TRUE(IS_TAG_ENABLED(bar));
    EXPECT_TRUE(IS_TAG_ENABLED(baz));

    txvc_log_configure("all+baz-", LOG_LEVEL_VERBOSE, false);
    EXPECT_TRUE(IS_TAG_ENABLED(foo));
    EXPECT_TRUE(IS_TAG_ENABLED(bar));
    EXPECT_FALSE(IS_TAG_ENABLED(baz));

#undef IS_TAG_ENABLED
}

TEST_CASE(EnableTagThenDisable_TagStateFollowsTheMostRecentConfig) {
    struct txvc_log_tag tag = TXVC_LOG_TAG_INITIALIZER(foo);
    txvc_log_configure("foo+", LOG_LEVEL_VERBOSE, false);
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    txvc_log_configure("foo-", LOG_LEVEL_VERBOSE, false);
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    txvc_log_configure("foo+", LOG_LEVEL_VERBOSE, false);
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    EXPECT_TRUE(txvc_log_tag_enabled(&tag));
    txvc_log_configure("foo-", LOG_LEVEL_VERBOSE, false);
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
    EXPECT_FALSE(txvc_log_tag_enabled(&tag));
}


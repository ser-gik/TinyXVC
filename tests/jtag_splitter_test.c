/* SPDX-License-Identifier: BSD-2-Clause */

#include "ttest/test.h"

#include "txvc/jtag_splitter.h"

TEST_SUITE(JtagSplitter)

static struct txvc_jtag_splitter gUut;

static struct mock {
    int dummy;
} gMock;

static bool mock_splitter_callback(const struct txvc_jtag_split_event *event, void* extra) {
    (void) event;
    (void) extra;
    return true;
}

DO_BEFORE_EACH_CASE() {
    ASSERT_TRUE(txvc_jtag_splitter_init(&gUut, mock_splitter_callback, &gMock));
}

DO_AFTER_EACH_CASE() {
    ASSERT_TRUE(txvc_jtag_splitter_deinit(&gUut));
}

/*
 * TODO implement tests
 */

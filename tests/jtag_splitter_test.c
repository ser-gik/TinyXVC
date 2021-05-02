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

#include "txvc/jtag_splitter.h"

TEST_SUITE(JtagSplitter)

static struct txvc_jtag_splitter gUut;

static struct mock {
    int i;



} gMock;


static bool mock_tms_sender(const uint8_t* tms, int fromBitIdx, int toBitIdx, void* extra) {
    (void) tms;
    (void) fromBitIdx;
    (void) toBitIdx;
    (void) extra;

    return true;
}

static bool mock_tdi_sender(const uint8_t* tdi, uint8_t* tdo, int fromBitIdx, int toBitIdx,
                            bool lastTmsBitHigh, void* extra) {
    (void) tdi;
    (void) tdo;
    (void) fromBitIdx;
    (void) toBitIdx;
    (void) lastTmsBitHigh;
    (void) extra;

    return true;
}

DO_BEFORE_EACH_CASE() {
    ASSERT_TRUE(txvc_jtag_splitter_init(&gUut, mock_tms_sender, &gMock, mock_tdi_sender, &gMock));
}

DO_AFTER_EACH_CASE() {
    ASSERT_TRUE(txvc_jtag_splitter_deinit(&gUut));
}

TEST_CASE(Dummy) {
    FAIL_FATAL("Implement me");
}


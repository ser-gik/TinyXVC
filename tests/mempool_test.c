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
#include "txvc/mempool.h"

TEST_SUITE(Mempool)

static struct txvc_mempool gUut;

DO_BEFORE_EACH_CASE() {
    txvc_mempool_init(&gUut, 512);
}

DO_AFTER_EACH_CASE() {
    txvc_mempool_deinit(&gUut);
}

TEST_CASE(AllocDifferentSizes_BlockAllocatedNoCrash) {
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 1) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 3) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 5) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 7) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 13) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 400) != NULL);
}

TEST_CASE(AllocEdgeCases_Ok) {
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 0) == NULL);
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 512) != NULL);
}

TEST_CASE(AllocAllreclaimAllocAgain_BlocksAllocatedNoCrash) {
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 512) != NULL);
    txvc_mempool_reclaim_all(&gUut);
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 256) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc(&gUut, 256) != NULL);
}


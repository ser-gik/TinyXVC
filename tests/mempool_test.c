/* SPDX-License-Identifier: BSD-2-Clause */

#include "ttest/test.h"
#include "txvc/mempool.h"

#include <stdbool.h>
#include <stdint.h>

TEST_SUITE(Mempool)

static struct txvc_mempool gUut;

static inline bool ptr_is_aligned(void *p, unsigned align) {
    return (((uintptr_t) p) % align) == 0;
}

DO_BEFORE_EACH_CASE() {
    txvc_mempool_init(&gUut, 512);
}

DO_AFTER_EACH_CASE() {
    txvc_mempool_deinit(&gUut);
}

TEST_CASE(AllocDifferentSizes_BlockAllocatedNoCrash) {
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 1) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 3) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 5) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 7) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 13) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 400) != NULL);
}

TEST_CASE(AllocEdgeCases_Ok) {
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 0) == NULL);
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 512) != NULL);
}

TEST_CASE(AllocAllreclaimAllocAgain_BlocksAllocatedNoCrash) {
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 512) != NULL);
    txvc_mempool_reclaim_all(&gUut);
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 256) != NULL);
    ASSERT_TRUE(txvc_mempool_alloc_unaligned(&gUut, 256) != NULL);
}

TEST_CASE(AllocAligned_ResultIsAlignedCorrectly) {
    ASSERT_TRUE(ptr_is_aligned(txvc_mempool_alloc_aligned(&gUut, 1, 1), 1));
    txvc_mempool_alloc_unaligned(&gUut, 1);
    ASSERT_TRUE(ptr_is_aligned(txvc_mempool_alloc_aligned(&gUut, 2, 2), 2));
    txvc_mempool_alloc_unaligned(&gUut, 1);
    ASSERT_TRUE(ptr_is_aligned(txvc_mempool_alloc_aligned(&gUut, 4, 4), 4));
    txvc_mempool_alloc_unaligned(&gUut, 1);
    ASSERT_TRUE(ptr_is_aligned(txvc_mempool_alloc_aligned(&gUut, 8, 8), 8));
    txvc_mempool_alloc_unaligned(&gUut, 1);
    ASSERT_TRUE(ptr_is_aligned(txvc_mempool_alloc_aligned(&gUut, 16, 16), 16));
    txvc_mempool_alloc_unaligned(&gUut, 1);
    struct foo { int i; };
    ASSERT_TRUE(ptr_is_aligned(txvc_mempool_alloc_object(&gUut, struct foo), alignof(struct foo)));
}


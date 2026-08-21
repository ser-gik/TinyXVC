/* SPDX-License-Identifier: BSD-2-Clause */

#include "txvc/mempool.h"

#include "txvc/defs.h"
#include "txvc/log.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

TXVC_DEFAULT_LOG_TAG(mempool);

static void mallocPoolRelease(unsigned char *start, unsigned char *end) {
    TXVC_UNUSED(end);
    free(start);
}

void txvc_mempool_init(struct txvc_mempool *mempool, size_t sz) {
    void *p = malloc(sz);
    if (!p) {
        FATAL("Can not create %zu-bytes mempool\n", sz);
    }
    mempool->start = p;
    mempool->end = mempool->start + sz;
    mempool->head = mempool->start;
    mempool->fatalOom = 1;
    mempool->release = mallocPoolRelease;
}

void txvc_mempool_deinit(struct txvc_mempool *mempool) {
    if (mempool->release) {
        mempool->release(mempool->start, mempool->end);
        mempool->release = NULL;
    }
}

unsigned char *txvc_mempool_alloc_unaligned(struct txvc_mempool *mempool, size_t sz) {
    if (sz == 0) {
        return NULL;
    }
    if (mempool->head + sz <= mempool->end) {
        void *ret = mempool->head;
        mempool->head += sz;
        return ret;
    }
    if (mempool->fatalOom) {
        FATAL("OOM at %zu-bytes mempool\n", mempool->end - mempool->start);
    }
    return NULL;
}

unsigned char *txvc_mempool_alloc_aligned(struct txvc_mempool *mempool, size_t sz, size_t align) {
    if (sz == 0 || align == 0) {
        return NULL;
    }
    const size_t alignMask = align - 1u;
    ALWAYS_ASSERT((align & alignMask) == 0);
    uintptr_t mod = ((uintptr_t) mempool->head) & alignMask;
    unsigned char *adjustedHead = mempool->head + (mod ? align - mod : 0u);
    if (adjustedHead + sz <= mempool->end) {
        void *ret = adjustedHead;
        mempool->head = adjustedHead + sz;
        return ret;
    }
    if (mempool->fatalOom) {
        FATAL("OOM at %zu-bytes mempool\n", mempool->end - mempool->start);
    }
    return NULL;
}
  
void txvc_mempool_reclaim_all(struct txvc_mempool *mempool) {
    mempool->head = mempool->start;
}


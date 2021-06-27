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

void txvc_spanpool_init(struct txvc_spanpool *pool, size_t sz) {
    void *p = malloc(sz);
    if (!p) {
        FATAL("Can not create %zu-bytes spanpool\n", sz);
    }
    pool->start = p;
    pool->end = pool->start + sz;
    pool->head = pool->start;
    pool->spanOpen = 0;
    pool->fatalOom = 1;
    pool->release = mallocPoolRelease;
}

void txvc_spanpool_deinit(struct txvc_spanpool *pool) {
    if (pool->release) {
        pool->release(pool->start, pool->end);
        pool->release = NULL;
    }
}

unsigned char *txvc_spanpool_open_span_unaligned(struct txvc_spanpool *pool) {
    ALWAYS_ASSERT(!pool->spanOpen);
    pool->spanOpen = 1;
    return pool->head;
}

unsigned char *txvc_spanpool_span_enlarge(struct txvc_spanpool *pool, size_t delta) {
    ALWAYS_ASSERT(pool->spanOpen);
    if (pool->head + delta <= pool->end) {
        unsigned char *ret = pool->head;
        pool->head += delta;
        return ret;
    }
    if (pool->fatalOom) {
        FATAL("OOM at %zu-bytes spanpool\n", pool->end - pool->start);
    }
    return NULL;
}

void txvc_spanpool_close_span(struct txvc_spanpool *pool) {
    ALWAYS_ASSERT(pool->spanOpen);
    pool->spanOpen = 0;
}

void txvc_spanpool_reclaim_all(struct txvc_spanpool *pool) {
    pool->head = pool->start;
}


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
    mempool->growDownward = 0;
    mempool->fatalOom = 1;
    mempool->release = mallocPoolRelease;
}

void txvc_mempool_deinit(struct txvc_mempool *mempool) {
    if (mempool->release) {
        mempool->release(mempool->start, mempool->end);
        mempool->release = NULL;
    }
}

void *txvc_mempool_alloc(struct txvc_mempool *mempool, size_t sz) {
    if (sz == 0) {
        return NULL;
    }
    if (mempool->growDownward) {
        if (mempool->start + sz <= mempool->head) {
            mempool->head -= sz;
            return mempool->head;
        }
    } else {
        if (mempool->head + sz <= mempool->end) {
            void *ret = mempool->head;
            mempool->head += sz;
            return ret;
        }
    }

    if (mempool->fatalOom) {
        FATAL("OOM at %zu-bytes mempool\n", mempool->end - mempool->start);
    }
    return NULL;
}

void txvc_mempool_reclaim_all(struct txvc_mempool *mempool) {
    mempool->head = mempool->growDownward ? mempool->end : mempool->start;
}


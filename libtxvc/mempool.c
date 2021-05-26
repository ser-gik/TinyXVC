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

#include "txvc/log.h"

TXVC_DEFAULT_LOG_TAG(mempool);

void *txvc_mempool_alloc(struct txvc_mempool *mempool, size_t sz) {
    if (mempool->_growDownward) {
        if (mempool->_bufferStart + sz <= mempool->_head) {
            mempool->_head -= sz;
            return mempool->_head;
        }
    } else {
        if (mempool->_head + sz <= mempool->_bufferEnd) {
            void *ret = mempool->_head;
            mempool->_head += sz;
            return ret;
        }
    }

    if (mempool->_fatalOom) {
        FATAL("OOM at %zu-bytes mempool\n", mempool->_bufferEnd - mempool->_bufferStart);
    }
    return NULL;
}

void txvc_mempool_reclaim_all(struct txvc_mempool *mempool) {
    mempool->_head = mempool->_growDownward ? mempool->_bufferEnd : mempool->_bufferStart;
}


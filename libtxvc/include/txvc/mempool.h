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

#pragma once

#include <stddef.h>
#include <stdalign.h>

/**
 * Simple linear arena allocator.
 * Allocated blocks can be freed only all at once.
 *
 * User MUST NOT access any field directly.
 */
struct txvc_mempool {
    unsigned char *start;
    unsigned char *end;
    unsigned char *head;
    unsigned fatalOom : 1;
    void (*release)(unsigned char *start, unsigned char *end);
};

extern void txvc_mempool_init(struct txvc_mempool *mempool, size_t sz);
extern void txvc_mempool_deinit(struct txvc_mempool *mempool);

extern unsigned char *txvc_mempool_alloc_unaligned(struct txvc_mempool *mempool, size_t sz);
extern unsigned char *txvc_mempool_alloc_aligned(struct txvc_mempool *mempool,
        size_t sz, size_t align);
#define txvc_mempool_alloc_object(mempool, type) \
    (type *) (txvc_mempool_alloc_aligned(mempool, sizeof(type), alignof(type)))
extern void txvc_mempool_reclaim_all(struct txvc_mempool *mempool);


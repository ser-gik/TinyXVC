/* SPDX-License-Identifier: BSD-2-Clause */

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


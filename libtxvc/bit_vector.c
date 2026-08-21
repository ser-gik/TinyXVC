/* SPDX-License-Identifier: BSD-2-Clause */

#include "txvc/bit_vector.h"

#include "txvc/log.h"

#include <unistd.h>
#include <fcntl.h>

#include <stdint.h>

TXVC_DEFAULT_LOG_TAG(bitVector);

static inline bool get_bit(const uint8_t* p, int idx) {
    return !!(p[idx / 8] & (1 << (idx % 8)));
}

void txvc_bit_vector_random(uint8_t* out, int outSz) {
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    ALWAYS_ASSERT(fd >= 0);
    int rd = read(fd, out, outSz);
    ALWAYS_ASSERT(rd == outSz);
    close(fd);
}

bool txvc_bit_vector_equal(
        const uint8_t* lhs, int lhsStart, int lhsEnd,
        const uint8_t* rhs, int rhsStart, int rhsEnd) {
    if (lhsEnd - lhsStart != rhsEnd - rhsStart) return false;
    while (lhsStart < lhsEnd) {
        if (get_bit(lhs, lhsStart) != get_bit(rhs, rhsStart)) return false;
        lhsStart++;
        rhsStart++;
    }
    return true;
}

int txvc_bit_vector_format_lsb(char* out, int outSz, const uint8_t* vector, int start, int end) {
    char* p = out;
    int avail = outSz;
    for (int idx = start; idx < end; ) {
        const uint8_t octet = vector[idx / 8];
        do {
            if (avail <= 1) break;
            *p = (octet & (1 << (idx % 8))) ? '1' : '0';
            p++;
            avail--;
            idx++;
        } while ((idx < end) && (idx % 8) != 0);
    }
    if (avail <= 0) out[outSz - 1] = '\0';
    else *p = '\0';
    return end - start;
}

int txvc_bit_vector_format_msb(char* out, int outSz, const uint8_t* vector, int start, int end) {
    char* p = out;
    int avail = outSz;
    for (int idx = end - 1; idx >= start; ) {
        const uint8_t octet = vector[idx / 8];
        do {
            if (avail <= 1) break;
            *p = (octet & (1 << (idx % 8))) ? '1' : '0';
            p++;
            avail--;
            idx--;
        } while ((idx >= start) && (idx % 8) != 7);
    }
    if (avail <= 0) out[outSz - 1] = '\0';
    else *p = '\0';
    return end - start;
}

/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "defs.h"

extern void txvc_bit_vector_random(uint8_t* out, int outSz);

extern bool txvc_bit_vector_equal(
        const uint8_t* lhs, int lhsStart, int lhsEnd,
        const uint8_t* rhs, int rhsStart, int rhsEnd);

extern int txvc_bit_vector_format_lsb(char* out, int outSz, const uint8_t* vector, int start, int end);
extern int txvc_bit_vector_format_msb(char* out, int outSz, const uint8_t* vector, int start, int end);


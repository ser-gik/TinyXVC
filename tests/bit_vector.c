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

#include "txvc/bit_vector.h"

#include <stdint.h>

TEST_SUITE(BitVector)

TEST_CASE(Comparison) {
#define VEC(start, end, ...) (uint8_t[]){ __VA_ARGS__ }, start, end
    EXPECT_TRUE(txvc_bit_vector_equal(VEC(0, 8, 0x00), VEC(0, 8, 0x00)));
    EXPECT_TRUE(txvc_bit_vector_equal(VEC(0, 8, 0xff), VEC(0, 8, 0xff)));
    EXPECT_TRUE(txvc_bit_vector_equal(VEC(4, 8, 0xac), VEC(0, 4, 0xea)));
    EXPECT_FALSE(txvc_bit_vector_equal(VEC(7, 8, 0x80), VEC(7, 8, 0x7f)));
    EXPECT_TRUE(txvc_bit_vector_equal(VEC(8, 24, 0x12, 0x34, 0x56), VEC(0, 16, 0x34, 0x56)));
    EXPECT_FALSE(txvc_bit_vector_equal(VEC(7, 23, 0x12, 0x34, 0x56), VEC(0, 16, 0x34, 0x56)));
    EXPECT_FALSE(txvc_bit_vector_equal(VEC(0, 7, 0x00), VEC(0, 8, 0x00)));
#undef VEC
}

TEST_CASE(Formatting) {
    char formatted[128];
#define FORMAT_LSB(start, end, ...) \
    txvc_bit_vector_format_lsb(formatted, sizeof(formatted), (uint8_t[]){ __VA_ARGS__ }, start, end)
#define FORMAT_MSB(start, end, ...) \
    txvc_bit_vector_format_msb(formatted, sizeof(formatted), (uint8_t[]){ __VA_ARGS__ }, start, end)
    FORMAT_LSB(0, 8, 0xa5);
    EXPECT_EQ(CSTR("10100101"), CSTR(formatted));
    FORMAT_MSB(0, 8, 0xa5);
    EXPECT_EQ(CSTR("10100101"), CSTR(formatted));
    FORMAT_LSB(0, 7, 0xff, 0xff);
    EXPECT_EQ(CSTR("1111111"), CSTR(formatted));
    FORMAT_MSB(0, 7, 0xff, 0xff);
    EXPECT_EQ(CSTR("1111111"), CSTR(formatted));
    FORMAT_LSB(0, 9, 0xff, 0xff);
    EXPECT_EQ(CSTR("111111111"), CSTR(formatted));
    FORMAT_MSB(0, 9, 0xff, 0xff);
    EXPECT_EQ(CSTR("111111111"), CSTR(formatted));
    FORMAT_LSB(0, 16, 0x12, 0x34);
    EXPECT_EQ(CSTR("0100100000101100"), CSTR(formatted));
    FORMAT_MSB(0, 16, 0x12, 0x34);
    EXPECT_EQ(CSTR("0011010000010010"), CSTR(formatted));
    FORMAT_LSB(1, 14, 0x12, 0x34);
    EXPECT_EQ(CSTR("1001000001011"), CSTR(formatted));
    FORMAT_MSB(1, 14, 0x12, 0x34);
    EXPECT_EQ(CSTR("1101000001001"), CSTR(formatted));
    FORMAT_LSB(0, 24, 0x00, 0xff, 0x00);
    EXPECT_EQ(CSTR("000000001111111100000000"), CSTR(formatted));
    FORMAT_MSB(0, 24, 0x00, 0xff, 0x00);
    EXPECT_EQ(CSTR("000000001111111100000000"), CSTR(formatted));
#undef FORMAT_LSB
#undef FORMAT_MSB
}

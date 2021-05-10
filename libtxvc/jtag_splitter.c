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

#include <stdint.h>
#define TXVC_JTAG_SPLITTER_IMPL

#include "txvc/jtag_splitter.h"

#include "txvc/log.h"
#include "txvc/defs.h"
#include "txvc/bit_vector.h"

TXVC_DEFAULT_LOG_TAG(jtagSplit);

#define JTAG_STATES(X) \
    X(TEST_LOGIC_RESET) \
    X(RUN_TEST_IDLE) \
    X(SELECT_DR_SCAN) \
    X(CAPTURE_DR) \
    X(SHIFT_DR) \
    X(EXIT_1_DR) \
    X(PAUSE_DR) \
    X(EXIT_2_DR) \
    X(UPDATE_DR) \
    X(SELECT_IR_SCAN) \
    X(CAPTURE_IR) \
    X(SHIFT_IR) \
    X(EXIT_1_IR) \
    X(PAUSE_IR) \
    X(EXIT_2_IR) \
    X(UPDATE_IR)

enum jtag_state {
#define AS_ENUM_MEMBER(name) name,
    JTAG_STATES(AS_ENUM_MEMBER)
#undef AS_ENUM_MEMBER
};

static const char* jtag_state_name(enum jtag_state state) {
    switch (state) {
#define AS_NAME_CASE(name) case name: return #name;
        JTAG_STATES(AS_NAME_CASE)
#undef AS_NAME_CASE
        default:
            return "???";
    }
}

static bool tapReset(txvc_jtag_splitter_tms_sender_fn tmsSender, void* tmsSenderExtra) {
    const uint8_t tmsTapResetVector = 0x1f;
    return tmsSender(&tmsTapResetVector, 0, 5, tmsSenderExtra);
}

static enum jtag_state next_state(enum jtag_state curState, bool tmsHigh) {
    switch (curState) {
        case TEST_LOGIC_RESET: return tmsHigh ? TEST_LOGIC_RESET : RUN_TEST_IDLE;
        case RUN_TEST_IDLE: return tmsHigh ? SELECT_DR_SCAN : RUN_TEST_IDLE;
        case SELECT_DR_SCAN: return tmsHigh ? SELECT_IR_SCAN : CAPTURE_DR;
        case CAPTURE_DR: return tmsHigh ? EXIT_1_DR : SHIFT_DR;
        case SHIFT_DR: return tmsHigh ? EXIT_1_DR : SHIFT_DR;
        case EXIT_1_DR: return tmsHigh ? UPDATE_DR : PAUSE_DR;
        case PAUSE_DR: return tmsHigh ? EXIT_2_DR : PAUSE_DR;
        case EXIT_2_DR: return tmsHigh ? UPDATE_DR : SHIFT_DR;
        case UPDATE_DR: return tmsHigh ? SELECT_DR_SCAN : RUN_TEST_IDLE;
        case SELECT_IR_SCAN: return tmsHigh ? TEST_LOGIC_RESET : CAPTURE_IR;
        case CAPTURE_IR: return tmsHigh ? EXIT_1_IR : SHIFT_IR;
        case SHIFT_IR: return tmsHigh ? EXIT_1_IR : SHIFT_IR;
        case EXIT_1_IR: return tmsHigh ? UPDATE_IR : PAUSE_IR;
        case PAUSE_IR: return tmsHigh ? EXIT_2_IR : PAUSE_IR;
        case EXIT_2_IR: return tmsHigh ? UPDATE_IR : SHIFT_IR;
        case UPDATE_IR: return tmsHigh ? SELECT_DR_SCAN : RUN_TEST_IDLE;
    }
    TXVC_UNREACHABLE();
}

bool txvc_jtag_splitter_init(struct txvc_jtag_splitter* splitter,
        txvc_jtag_splitter_tms_sender_fn tmsSender, void* tmsSenderExtra,
        txvc_jtag_splitter_tdi_sender_fn tdiSender, void* tdiSenderExtra) {
    if (!tapReset(tmsSender, tmsSenderExtra)) {
        ERROR("Can not reset TAP\n");
        return false;
    }
    splitter->state = TEST_LOGIC_RESET;
    splitter->tmsSender = tmsSender;
    splitter->tmsSenderExtra = tmsSenderExtra;
    splitter->tdiSender = tdiSender;
    splitter->tdiSenderExtra = tdiSenderExtra;
    return true;
}

bool txvc_jtag_splitter_deinit(struct txvc_jtag_splitter* splitter) {
    if (!tapReset(splitter->tmsSender, splitter->tmsSenderExtra)) {
        ERROR("Can not reset TAP\n");
        return false;
    }
    splitter->state = TEST_LOGIC_RESET;
    return true;
}

bool txvc_jtag_splitter_process(struct txvc_jtag_splitter* splitter,
        int numBits, const uint8_t* tms, const uint8_t* tdi, uint8_t* tdo) {
    int firstPendingBitIdx = 0;
    enum jtag_state jtagState = splitter->state;
    for (int bitIdx = 0; bitIdx < numBits;) {
        uint8_t tmsByte = tms[bitIdx / 8];
        const int thisRoundEndBitIdx = bitIdx + 8 > numBits ? numBits :bitIdx + 8;
        for (; bitIdx < thisRoundEndBitIdx; tmsByte >>= 1, bitIdx++) {
            const bool tmsBit = tmsByte & 1;
            const enum jtag_state nextJtagState = next_state(jtagState, tmsBit);
            const bool isShift = jtagState == SHIFT_DR || jtagState == SHIFT_IR;
            const bool nextIsShift = nextJtagState == SHIFT_DR || nextJtagState == SHIFT_IR;
            const bool enteringShift = !isShift && nextIsShift;
            const bool leavingShift = isShift && !nextIsShift;
            const bool flush = bitIdx == numBits - 1 || enteringShift || leavingShift;
            if (flush) {
                const int nextPendingBitIdx = bitIdx + 1;
                if (isShift) {
                    if (!splitter->tdiSender(tdi, tdo, firstPendingBitIdx, nextPendingBitIdx,
                                tmsBit, splitter->tdiSenderExtra)) {
                        goto bail_reset;
                    }
                    if (VERBOSE_ENABLED) {
                        char buf[1024];
                        const char* logPrefix = tmsBit ? "shift" : "partial shift";
                        const int numBitsShifted = nextPendingBitIdx - firstPendingBitIdx;
                        if (numBitsShifted > (int) sizeof(buf) - 1) {
                            VERBOSE("%s in:  (%d bits)\n", logPrefix, numBitsShifted);
                            VERBOSE("%s out: (%d bits)\n", logPrefix, numBitsShifted);
                        } else {
                            txvc_bit_vector_format_msb(buf, sizeof(buf), tdi,
                                    firstPendingBitIdx, nextPendingBitIdx);
                            VERBOSE("%s in:  %s\n", logPrefix, buf);
                            txvc_bit_vector_format_msb(buf, sizeof(buf), tdo,
                                    firstPendingBitIdx, nextPendingBitIdx);
                            VERBOSE("%s out: %s\n", logPrefix, buf);
                        }
                    }
                } else {
                    if (!splitter->tmsSender(tms, firstPendingBitIdx, nextPendingBitIdx,
                                splitter->tmsSenderExtra)) {
                        goto bail_reset;
                    }
                }
                firstPendingBitIdx = nextPendingBitIdx;
            }
            if (VERBOSE_ENABLED && jtagState != nextJtagState) {
                VERBOSE("%s\n", jtag_state_name(nextJtagState));
            }
            jtagState = nextJtagState;
        }
    }
    splitter->state = jtagState;
    return true;

bail_reset:
    WARN("Resetting TAP\n");
    tapReset(splitter->tmsSender, splitter->tmsSenderExtra);
    splitter->state = TEST_LOGIC_RESET;
    return false;
}


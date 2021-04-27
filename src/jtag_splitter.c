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

#include "jtag_splitter.h"

#include "log.h"
#include "txvc_defs.h"
#include "bit_vector.h"

TXVC_DEFAULT_LOG_TAG(jtag-split);

#define JTAG_STATES(X) \
    X(TestLogicReset) \
    X(RunTestIdle) \
    X(SelectDRScan) \
    X(CaptureDR) \
    X(ShiftDR) \
    X(Exit1DR) \
    X(PauseDR) \
    X(Exit2DR) \
    X(UpdateDR) \
    X(SelectIRScan) \
    X(CaptureIR) \
    X(ShiftIR) \
    X(Exit1IR) \
    X(PauseIR) \
    X(Exit2IR) \
    X(UpdateIR)

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
        case TestLogicReset: return tmsHigh ? TestLogicReset : RunTestIdle;
        case RunTestIdle: return tmsHigh ? SelectDRScan : RunTestIdle;
        case SelectDRScan: return tmsHigh ? SelectIRScan : CaptureDR;
        case CaptureDR: return tmsHigh ? Exit1DR : ShiftDR;
        case ShiftDR: return tmsHigh ? Exit1DR : ShiftDR;
        case Exit1DR: return tmsHigh ? UpdateDR : PauseDR;
        case PauseDR: return tmsHigh ? Exit2DR : PauseDR;
        case Exit2DR: return tmsHigh ? UpdateDR : ShiftDR;
        case UpdateDR: return tmsHigh ? SelectDRScan : RunTestIdle;
        case SelectIRScan: return tmsHigh ? TestLogicReset : CaptureIR;
        case CaptureIR: return tmsHigh ? Exit1IR : ShiftIR;
        case ShiftIR: return tmsHigh ? Exit1IR : ShiftIR;
        case Exit1IR: return tmsHigh ? UpdateIR : PauseIR;
        case PauseIR: return tmsHigh ? Exit2IR : PauseIR;
        case Exit2IR: return tmsHigh ? UpdateIR : ShiftIR;
        case UpdateIR: return tmsHigh ? SelectDRScan : RunTestIdle;
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
    splitter->state = TestLogicReset;
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
    splitter->state = TestLogicReset;
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
            const bool isShift = jtagState == ShiftDR || jtagState == ShiftIR;
            const bool nextIsShift = nextJtagState == ShiftDR || nextJtagState == ShiftIR;
            const bool enteringShift = !isShift && nextIsShift;
            const bool leavingShift = isShift && !nextIsShift;
            const bool flush = bitIdx == numBits - 1 || enteringShift || leavingShift;
            if (flush) {
                const int nextPendingBitIdx = bitIdx + 1;
                if (isShift) {

                    /*
                    for (int i = firstPendingBitIdx; i < nextPendingBitIdx; i++) {
                        if (!splitter->tdiSender(tdi, tdo, i, i + 1,
                                    i == nextPendingBitIdx - 1 ? tmsBit : false, splitter->tdiSenderExtra)) {
                            goto bail_reset;
                        }
                    }
                    */



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
    splitter->state = TestLogicReset;
    return false;
}


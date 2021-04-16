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

#define TXVC_JTAG_SPLITTER_IMPL

#include "jtag_splitter.h"

#include "log.h"
#include "txvc_defs.h"

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


static bool get_bit(const uint8_t* vector, int idx) {
    return vector[idx / 8] & (1u << (idx % 8));
}

static void set_bit(uint8_t* vector, int idx, bool bit) {
    if (bit) vector[idx / 8] |= 1u << (idx % 8);
    else vector[idx / 8] &= ~(1u << (idx % 8));
}

static void copy_bits(const uint8_t* src, int srcIdx, uint8_t* dst, int dstIdx, int numBits) {
    for (int i = 0; i < numBits; i++) {
        set_bit(dst, dstIdx++, get_bit(src, srcIdx++));
    }
}

static bool tapReset(txvc_jtag_splitter_tms_sender_fn tmsSender, void* tmsSenderExtra) {
    const uint8_t tmsTapResetVector = 0x1f;
    const int tmsTapResetVectorLen = 5;
    return tmsSender(tmsTapResetVectorLen, &tmsTapResetVector, tmsSenderExtra);
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
    uint8_t vector[4096];
    const int maxVectorBits = sizeof(vector) * 8;
    int vectorBits = 0;

    enum jtag_state curState = splitter->state;

    for (int i = 0; i < numBits; i++) {
        const bool tmsBit = get_bit(tms, i);
        const bool tdiBit = get_bit(tdi, i);
        set_bit(tdo, i, false);
        const enum jtag_state nextState = next_state(curState, tmsBit);
        if (VERBOSE_ENABLED && curState != nextState) {
            VERBOSE("%s => %s\n", jtag_state_name(curState), jtag_state_name(nextState));
        }
        const bool inShift = curState == ShiftDR || curState == ShiftIR;
        const bool enteringShift = !inShift && (nextState == ShiftDR || nextState == ShiftIR);
        const bool exitingShift = inShift && (nextState != ShiftDR && nextState != ShiftIR);

        set_bit(vector, vectorBits++, inShift ? tdiBit : tmsBit);

        const bool flush =
               i == numBits - 1
            || vectorBits == maxVectorBits
            || enteringShift
            || exitingShift;

        if (flush) {
            if (inShift) {
                VERBOSE("flush %d TDI bits\n", vectorBits);
                uint8_t tdoVector[sizeof(vector)];
                if (!splitter->tdiSender(vectorBits, vector, tdoVector, tmsBit,
                            splitter->tdiSenderExtra)) {
                    goto bail_reset;
                }
                copy_bits(tdoVector, 0, tdo, i - vectorBits + 1, vectorBits);
            } else {
                VERBOSE("flush %d TMS bits\n", vectorBits);
                if (!splitter->tmsSender(vectorBits, vector, splitter->tmsSenderExtra)) {
                    goto bail_reset;
                }
            }
            vectorBits = 0;
        }

        curState = nextState;
    }
    splitter->state = curState;
    return true;

bail_reset:
    WARN("Resetting TAP\n");
    tapReset(splitter->tmsSender, splitter->tmsSenderExtra);
    splitter->state = TestLogicReset;
    return false;
}


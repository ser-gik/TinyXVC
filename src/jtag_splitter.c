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

enum jtag_state {
    Invalid,

    TestLogicReset,
    RunTestIdle,
    SelectDRScan,
    CaptureDR,
    ShiftDR,
    Exit1DR,
    PauseDR,
    Exit2DR,
    UpdateDR,
    SelectIRScan,
    CaptureIR,
    ShiftIR,
    Exit1IR,
    PauseIR,
    Exit2IR,
    UpdateIR,
};


static bool tapReset(txvc_jtag_splitter_tms_sender_fn tmsSender, void* tmsSenderExtra) {
    const uint8_t tmsTapResetVector = 0x1f;
    const int tmsTapResetVectorLen = 5;
    return tmsSender(tmsTapResetVectorLen, &tmsTapResetVector, tmsSenderExtra);
}

TXVC_USED
static enum jtag_state next_state(enum jtag_state curState, bool tmsHigh) {
    if (tmsHigh) {
        switch (curState) {
            case TestLogicReset: return Invalid;
            case RunTestIdle: return Invalid;
            case SelectDRScan: return Invalid;
            case CaptureDR: return Invalid;
            case ShiftDR: return Invalid;
            case Exit1DR: return Invalid;
            case PauseDR: return Invalid;
            case Exit2DR: return Invalid;
            case UpdateDR: return Invalid;
            case SelectIRScan: return Invalid;
            case CaptureIR: return Invalid;
            case ShiftIR: return Invalid;
            case Exit1IR: return Invalid;
            case PauseIR: return Invalid;
            case Exit2IR: return Invalid;
            case UpdateIR: return Invalid;
        }
    } else {
        switch (curState) {
            case TestLogicReset: return Invalid;
            case RunTestIdle: return Invalid;
            case SelectDRScan: return Invalid;
            case CaptureDR: return Invalid;
            case ShiftDR: return Invalid;
            case Exit1DR: return Invalid;
            case PauseDR: return Invalid;
            case Exit2DR: return Invalid;
            case UpdateDR: return Invalid;
            case SelectIRScan: return Invalid;
            case CaptureIR: return Invalid;
            case ShiftIR: return Invalid;
            case Exit1IR: return Invalid;
            case PauseIR: return Invalid;
            case Exit2IR: return Invalid;
            case UpdateIR: return Invalid;
        }
    }
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
    TXVC_UNUSED(splitter);
    TXVC_UNUSED(numBits);
    TXVC_UNUSED(tms);
    TXVC_UNUSED(tdi);
    TXVC_UNUSED(tdo);

    WARN("%s: unimplemented stub\n", __func__);

    for (int i = 0; i < numBits; i++) {
        const int byteIdx = i / 8;
        const int bitIdx = i % 8;
        const bool tmsBit = tms[byteIdx] & (1u << bitIdx);
        const bool tdiBit = tdi[byteIdx] & (1u << bitIdx);
        const enum jtag_state curState = splitter->state;
        const enum jtag_state nextState = next_state(curState, tmsBit);













        splitter->state = nextState;

    }









    return false;
}






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

#include <stdbool.h>
#include <stdint.h>

/**
 * User-provided function that sents TMS vector to a TAP. TDI must not change during this
 * transfer. Byte 0 is sent first, each byte is sent from LSB to MSB.
 */
typedef bool (*txvc_jtag_splitter_tms_sender_fn)(
        int numBits, const uint8_t* tms, void* extra);

/**
 * User-provided function that shifts TDI vector to a TAP and simultaneously fills TDO vector
 * with data received from TAP. TMS must always be low during this transfer, except for
 * the last bit to be sent, where actual TMS level is denoted by associated argument.
 * Byte 0 is sent first, each byte is sent from LSB to MSB.
 */
typedef bool (*txvc_jtag_splitter_tdi_sender_fn)(
        int numBits, const uint8_t* tdi, uint8_t* tdo, bool lastBitTmsHigh, void* extra);

struct txvc_jtag_splitter {
#ifdef TXVC_JTAG_SPLITTER_IMPL
#define PRIVATE(type, name) type name
#else
#define PRIVATE(type, name) type do_not_ever_use_me_directly_ ## name
#endif

    PRIVATE(int, state);
    PRIVATE(txvc_jtag_splitter_tms_sender_fn, tmsSender);
    PRIVATE(void*, tmsSenderExtra);
    PRIVATE(txvc_jtag_splitter_tdi_sender_fn, tdiSender);
    PRIVATE(void*, tdiSenderExtra);

#undef PRIVATE
};

extern bool txvc_jtag_splitter_init(struct txvc_jtag_splitter* splitter,
        txvc_jtag_splitter_tms_sender_fn tmsSender, void* tmsSenderExtra,
        txvc_jtag_splitter_tdi_sender_fn tdiSender, void* tdiSenderExtra);
extern bool txvc_jtag_splitter_deinit(struct txvc_jtag_splitter* splitter);

extern bool txvc_jtag_splitter_process(struct txvc_jtag_splitter* splitter,
        int numBits, const uint8_t* tms, const uint8_t* tdi, uint8_t* tdo);


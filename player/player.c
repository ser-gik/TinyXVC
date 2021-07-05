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

/*
 * This is an ad-hoc JTAG decoder for raw logical analyzer samples that are read from file.
 * It depends on jtag splitter internal logging.
 * TODO:
 * - rework it to not depend on splitter logs
 * - provide CLI to setup at least JTAG signal bit positions in input samples
 */

#include "txvc/log.h"
#include "txvc/defs.h"
#include "txvc/jtag_splitter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

static inline void set_bit(uint8_t* p, int idx, bool bit) {
    uint8_t* octet = p + idx / 8;
    if (bit) *octet |= 1 << (idx % 8);
    else *octet &= ~(1 << (idx % 8));
}

static bool noop_splitter_callback(const struct txvc_jtag_split_event *event, void* extra) {
    TXVC_UNUSED(event);
    TXVC_UNUSED(extra);
    return true;
}

int main(int argc, const char **argv) {
    TXVC_UNUSED(argc);
    TXVC_UNUSED(argv);
    txvc_log_configure("all+", LOG_LEVEL_VERBOSE, false);

    const char *jtagRawSaplesFile = argv[1];
    //const size_t bytesPerSample = 1;
    const size_t tckBitPos = 3;
    const size_t tmsBitPos = 0;
    const size_t tdiBitPos = 1;
    const size_t tdoBitPos = 2;

    FILE* jtagSamples = fopen(jtagRawSaplesFile, "rb");
    if (!jtagSamples) {
        fprintf(stderr, "Can not open %s: %s\n", jtagRawSaplesFile, strerror(errno));
        return 1;
    }

    struct txvc_jtag_splitter splitter;
    txvc_jtag_splitter_init(&splitter, noop_splitter_callback, NULL);

    uint8_t tms[1024];
    uint8_t tdi[sizeof(tms)];
    uint8_t tdo[sizeof(tms)];
    const size_t maxBits = sizeof(tms) * 8;
    size_t curBits = 0;
    bool lastTckSample = true;
    for (;;) {
        uint8_t rawSample;
        bool eof = false;
        if (fread(&rawSample, 1, 1, jtagSamples) != 1) {
            eof = feof(jtagSamples);
            if (!eof) {
                fprintf(stderr, "Can not read from %s: %s\n", jtagRawSaplesFile, strerror(errno));
                return 1;
            }
        }
        if (!eof) {
            const bool tckSample = rawSample & (1 << tckBitPos);
            if (!lastTckSample && tckSample) {
                const bool tmsSample = rawSample & (1 << tmsBitPos);
                set_bit(tms, curBits, tmsSample);
                const bool tdiSample = rawSample & (1 << tdiBitPos);
                set_bit(tdi, curBits, tdiSample);
                const bool tdoSample = rawSample & (1 << tdoBitPos);
                set_bit(tdo, curBits, tdoSample);
                curBits++;
            }
            lastTckSample = tckSample;
        }
        const bool bufferFull = curBits == (maxBits - 1);

        if (eof || bufferFull) {
            txvc_jtag_splitter_process(&splitter, curBits, tms, tdi, tdo);
            curBits = 0;
            if (eof) {
                return 0;
            }
        }
    }
    TXVC_UNREACHABLE();
}


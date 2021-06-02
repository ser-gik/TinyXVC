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

#include "driver_wrapper.h"

#include "txvc/log.h"

TXVC_DEFAULT_LOG_TAG(driverWrapper);

struct txvc_driver txvcDriverWrapper;

#define DEFAULT_TCK_PERIOD 1000

static int (*orig_set_tck_period)(int tckPeriodNs);
static bool (*orig_shift_bits)(int numBits,
        const uint8_t *tmsVector,
        const uint8_t *tdiVector,
        uint8_t *tdoVector
        );

static int noop_set_tck_period(int tckPeriodNs) {
    WARN("Ignoring new TCK period %dns\n", tckPeriodNs);
    return tckPeriodNs;
}

static int onetime_set_tck_period(int tckPeriodNs) {
    txvcDriverWrapper.set_tck_period = orig_set_tck_period;
    return orig_set_tck_period(tckPeriodNs);
}

static bool onetime_shift_bits(int numBits,
        const uint8_t *tmsVector,
        const uint8_t *tdiVector,
        uint8_t *tdoVector
        ) {
    if (txvcDriverWrapper.set_tck_period == onetime_set_tck_period) {
        extern const char *txvcProgname;
        WARN("Client did not set TCK period before shifting data\n");
        WARN("Using default value: %dns\n", DEFAULT_TCK_PERIOD);
        WARN("See \"%s --help\" to enforce other TCK period\n", txvcProgname);
        onetime_set_tck_period(DEFAULT_TCK_PERIOD);
    }
    txvcDriverWrapper.shift_bits = orig_shift_bits;
    return orig_shift_bits(numBits, tmsVector, tdiVector, tdoVector);
}

void txvc_driver_wrapper_setup(const struct txvc_driver *driver,
        int fixedTckPeriod) {
    txvcDriverWrapper = *driver;
    if (fixedTckPeriod > 0) {
        /* Set desired period and inhibit future changes */
        if (txvcDriverWrapper.set_tck_period(fixedTckPeriod) == fixedTckPeriod) {
            txvcDriverWrapper.set_tck_period = noop_set_tck_period;
        }
    } else {
        /* Wrap methods to monitor if client touches shift too early */
        orig_set_tck_period = txvcDriverWrapper.set_tck_period;
        txvcDriverWrapper.set_tck_period = onetime_set_tck_period;
        orig_shift_bits = txvcDriverWrapper.shift_bits;
        txvcDriverWrapper.shift_bits = onetime_shift_bits;
    }
}


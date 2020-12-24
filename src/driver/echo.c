/*
 * Copyright 2020 Sergey Guralnik
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

#include "driver.h"
#include "utils.h"

#include <stddef.h>
#include <string.h>

static bool activate(const char **argNames, const char **argValues){
    TXVC_UNUSED(argNames);
    TXVC_UNUSED(argValues);
    return true;
}

static bool deactivate(void){
    return true;
}

static int max_vector_bits(void){
    return 1024;
}

static int set_tck_period(int tckPeriodNs){
    return tckPeriodNs;
}

static bool shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    TXVC_UNUSED(tmsVector);
    memcpy(tdoVector, tdiVector, (size_t) (numBits / 8 + !!(numBits % 8)));
    return true;
}

TXVC_DRIVER(echo) = {
    .name = "echo",
    .help = "Simple loopback driver that forwards TDI vector to TDO. No real device is involved\n"
            "Parameters:\n   none\n",
    .activate = activate,
    .deactivate = deactivate,
    .max_vector_bits = max_vector_bits,
    .set_tck_period = set_tck_period,
    .shift_bits = shift_bits,
};


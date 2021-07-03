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

#include "drivers/drivers.h"
#include "txvc/driver.h"

#include <stddef.h>

extern const struct txvc_driver driver_echo;
extern const struct txvc_driver driver_ft2232h;
extern const struct txvc_driver driver_ftdi_generic;

static const struct txvc_driver * const gDrivers[] = {
    &driver_echo,
    &driver_ft2232h,
    &driver_ftdi_generic,
};
static const size_t gNumDrivers = sizeof(gDrivers) / sizeof(gDrivers[0]);

const struct txvc_driver* txvc_enumerate_drivers(
        bool (*fn)(const struct txvc_driver *d, const void *extra), const void *extra) {
    for (size_t i = 0; i < gNumDrivers; i++) {
        const struct txvc_driver *d = gDrivers[i];
        if(!fn(d, extra)) {
            return d;
        }
    }
    return NULL;
}


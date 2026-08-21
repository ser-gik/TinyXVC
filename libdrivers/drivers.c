/* SPDX-License-Identifier: BSD-2-Clause */

#include "drivers/drivers.h"
#include "txvc/driver.h"

#include <stddef.h>

extern const struct txvc_driver driver_echo;
extern const struct txvc_driver driver_ftdi_generic;

static const struct txvc_driver * const gDrivers[] = {
    &driver_echo,
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


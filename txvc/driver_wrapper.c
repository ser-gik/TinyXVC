/* SPDX-License-Identifier: BSD-2-Clause */

#include "driver_wrapper.h"

#include "txvc/log.h"

TXVC_DEFAULT_LOG_TAG(driverWrapper);

struct txvc_driver txvcDriverWrapper;

#define DEFAULT_TCK_PERIOD 100

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
        WARN("See \"%s -h\" to enforce other TCK period\n", txvcProgname);
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


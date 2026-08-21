/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include "defs.h"

#include <stdbool.h>
#include <stdint.h>

struct txvc_driver {
    const char *name;
    const char *help;

    bool (*activate)(int numArg, const char **argNames, const char **argValues);
    bool (*deactivate)(void);

    int (*max_vector_bits)(void);
    int (*set_tck_period)(int tckPeriodNs);
    bool (*shift_bits)(int numBits,
            const uint8_t *tmsVector,
            const uint8_t *tdiVector,
            uint8_t *tdoVector
            );
};


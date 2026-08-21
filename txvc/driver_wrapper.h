/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include "txvc/driver.h"

extern struct txvc_driver txvcDriverWrapper;

extern void txvc_driver_wrapper_setup(const struct txvc_driver *driver,
        int fixedTckPeriod);


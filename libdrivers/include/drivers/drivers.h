/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include "txvc/driver.h"

extern const struct txvc_driver* txvc_enumerate_drivers(
        bool (*fn)(const struct txvc_driver *d, const void *extra), const void *extra);

